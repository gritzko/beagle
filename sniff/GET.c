//  GET: checkout a commit tree from keeper.
//
//  Pipeline:
//    1. Resolve the baseline tree from the latest get/post/patch row
//       (NULL on first checkout).
//    2. Pre-flight classifier (`get_overlap_check`) merges baseline
//       and target tree path-lists via `KEEPu8ssDrain` and produces:
//         * a no-op overlay list (paths whose content is unchanged —
//           WRITE skips them so dirty user content survives);
//         * an unlink list (clean baseline-only paths the post-WRITE
//           step drops from the wt);
//         * a refusal when any incoming change would clobber a dirty
//           wt file.
//    3. WRITE pass: WALKTreeLazy over target → materialise files,
//       creating parent dirs as needed, stamping each with a shared
//       ron60 ts via utimensat.
//    4. Drain the unlink list.
//    5. Append one `get` ULOG row with the same ts; advance the
//       keeper-side per-branch tip via REFSAppendVerb.
//
#include "GET.h"

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/RON.h"
#include "abc/URI.h"
#include "dog/DOG.h"
#include "dog/DPATH.h"
#include "dog/HOME.h"
#include "dog/ROWS.h"
#include "dog/ULOG.h"
#include "graf/DAG.h"
#include "graf/GRAF.h"
#include "dog/git/GIT.h"
#include "keeper/REFS.h"
#include "keeper/RESOLVE.h"
#include "keeper/WALK.h"

#include "AT.h"
#include "CLASS.h"
#include "SNIFF.h"
#include "SUBS.h"
#include "dog/git/IGNO.h"

typedef struct {
    keeper        *k;
    u8cs           reporoot;
    ron60          ts;          // stamp to apply via utimensat
    struct timespec tv;         // same stamp in timespec form
    ok64           error;
    //  Newline-separated, lex-sorted path lists (subsets of the target
    //  tree).  WALKTreeLazy visits the target in the same order, so we
    //  advance these cursors in lockstep with the walk.
    //
    //   noop_cursor   — target sha matches baseline; preserve whatever
    //                   bytes are on disk (including dirty user edits).
    //   merges_cursor — wt has a real local edit; the merge drain will
    //                   weave-merge wt vs tgt afterwards, so checkout
    //                   must not clobber the wt content here.
    u8cs           noop_cursor;
    u8cs           merges_cursor;
    //  Submodule (`160000` gitlink) collector — one `<path>\t<hex>\n`
    //  row per `WALK_KIND_SUB` entry seen during the target walk.
    //  Drained by `get_drain_subs` after the parent's WRITE pass.
    u8bp           subs_out;
    //  Per-file blob scratch, reused across every `get_write_one` in
    //  the WRITE pass (the visitor runs bare per tree entry, so a
    //  per-call mmap would churn one syscall pair per file).  Mapped
    //  once with the other GET bufs, `u8bReset` before each fetch.
    //  256 MB cap (lazy COW) — see get_write_one for the rationale.
    u8bp           blob;
} get_ctx;

//  Per-file report verbs.  Encoded via abc/ok64; colors live in
//  dog/ULOG.c's `ULOG_VERB_COLORS` palette.
//
//    new   added in the new version (file didn't exist before)
//    upd   updated by GET (checkout overwrote previous baseline bytes)
//    mod   locally modified by user; GET did NOT touch it (kept as-is
//          either because target == baseline or the noop classifier
//          decided the user's bytes survive)
//    mrg   weave-merged (user had a local edit AND target differed —
//          3-way merge integrated both sides)
//    del   removed in the target tree
con ron60 GET_V_NEW  = 0x32a7b;     // "new"
con ron60 GET_V_UPD  = 0x39d28;     // "upd"
con ron60 GET_V_MOD  = 0x31ce8;     // "mod"
con ron60 GET_V_DEL  = 0x28a70;     // "del"
con ron60 GET_V_MRG  = 0x31dab;     // "mrg"
//  Internal classifier bucket — never user-facing; WRITE pass uses
//  it to skip identical-content paths whose on-disk bytes are clean.
//  No palette entry; never feeds into a status report.
con ron60 GET_V_NOOP = 0xcb3cf4;    // "noop"

//  Write one blob to disk, create dirs as needed, chmod if exec,
//  symlink if link; then stamp it with ctx->tv.  Caller is
//  responsible for the dirty-overlap pre-flight (see get_overlap_check
//  and GETCheckout); reaching here means no dirty collision was
//  detected.
static ok64 get_write_one(get_ctx *g, u8cs path, u8 kind, u8cp esha) {
    sane(g);
    keeper *k = g->k;

    a_path(fp);
    call(SNIFFFullpath, fp, g->reporoot, path);

    //  Probe before we touch disk: file-on-disk before the write means
    //  we're updating (`mod`), absence means it's a fresh path (`new`).
    //  One extra lstat per write — negligible next to the keeper-pull
    //  and the inevitable open/write.
    filestat _pre = {};
    b8 pre_present = (FILELStat(&_pre, $path(fp)) == OK);

    //  Reused 256 MB blob scratch from the GET context (mapped once
    //  for the whole WRITE pass — see get_ctx.blob): linux.git has
    //  generated headers >20 MB (drivers/gpu/drm/amd/include/asic_reg/
    //  dcn/*) that blew through the previous 16 MB malloc.  Anonymous
    //  mmap pages on demand, so cost is what's actually written.
    u8bp bbuf = g->blob;
    u8bReset(bbuf);
    u8 bt = 0;
    sha1 entry_sha = {};
    sha1Mv(&entry_sha, (sha1cp)esha);
    ok64 o = KEEPGetExact(&entry_sha, bbuf, &bt);
    if (o != OK) {
        //  Empty-blob short-circuit.  The well-known empty-blob sha
        //  e69de29b… isn't always stored as a packed object — git
        //  trees can reference it without an explicit blob entry —
        //  so a fresh-clone wt walk must materialise empty files
        //  without going through KEEPGet.  Symmetric with POST's
        //  zero-size handler (sniff/POST.c:128).
        static u8 const EMPTY_BLOB_SHA[20] = {
            0xe6, 0x9d, 0xe2, 0x9b, 0xb2, 0xd1, 0xd6, 0x43,
            0x4b, 0x8b, 0x29, 0xae, 0x77, 0x5a, 0xd8, 0xc2,
            0xe4, 0x8c, 0x53, 0x91
        };
        if (memcmp(entry_sha.data, EMPTY_BLOB_SHA, 20) == 0) {
            //  Unlink first so a stale symlink at this path is replaced
            //  outright; FILECreate's O_CREAT|O_TRUNC otherwise follows
            //  the symlink and clobbers its target instead of the path.
            FILEUnLink($path(fp));
            int fd = -1;
            ok64 co = FILECreate(&fd, $path(fp));
            if (co != OK) {
                fprintf(stderr,
                    "sniff: get: cannot write %.*s: %s "
                    "(a parent path component is not a directory?)\n",
                    (int)u8bDataLen(fp), (char const *)u8bDataHead(fp),
                    ok64str(co));
                return co;
            }
            FILEClose(&fd);
            if (kind == WALK_KIND_EXE) FILEChmod($path(fp), 0755);
            call(SNIFFAtStampPath, fp, g->ts);
            ulogrec rep = {.ts = g->ts,
                .verb = pre_present ? GET_V_UPD : GET_V_NEW};
            u8csMv(rep.uri.path, path);
            call(ROWSPrintRow, &rep, ROWS_NAV_CAT);
            done;
        }
        return o;
    }

    if (kind == WALK_KIND_LNK) {
        FILEUnLink($path(fp));
        u8bFeed1(bbuf, 0);   // NUL-terminate so $path(bbuf) is C-string-safe
        if (FILESymLink($path(bbuf), $path(fp)) != OK) {
            fail(SNIFFFAIL);
        }
    } else {
        //  Unlink first so a stale symlink at this path is replaced
        //  outright; FILECreate's O_CREAT|O_TRUNC otherwise follows
        //  the symlink and writes its target instead of the path
        //  itself (e.g. a tag transition that flips a tracked entry
        //  from mode 120000 to 100644).
        FILEUnLink($path(fp));
        int fd = -1;
        o = FILECreate(&fd, $path(fp));
        if (o != OK) {
            fprintf(stderr,
                "sniff: get: cannot write %.*s: %s "
                "(a parent path component is not a directory?)\n",
                (int)u8bDataLen(fp), (char const *)u8bDataHead(fp),
                ok64str(o));
            return o;
        }
        u8cs data = {u8bDataHead(bbuf), u8bIdleHead(bbuf)};
        o = FILEFeedAll(fd, data);
        FILEClose(&fd);
        if (o != OK) return o;
        if (kind == WALK_KIND_EXE)
            FILEChmod($path(fp), 0755);
    }

    call(SNIFFAtStampPath, fp, g->ts);
    {
        ulogrec rep = {.ts = g->ts,
            .verb = pre_present ? GET_V_UPD : GET_V_NEW};
        u8csMv(rep.uri.path, path);
        call(ROWSPrintRow, &rep, ROWS_NAV_CAT);
    }
    done;
}

static ok64 get_visit(u8cs path, u8 kind, u8cp esha, u8cs blob,
                      void0p vctx) {
    (void)blob;  // lazy mode
    get_ctx *g = (get_ctx *)vctx;

    //  Submodule (gitlink, mode 160000).  Record `<path>\t<hex>\n` so
    //  the post-walk drain (`get_drain_subs`) can mount each sub via
    //  a recursive `be get`.  Materialise the mount-point dir now so
    //  recursion has a place to chdir into.  No blob write, no stamp.
    if (kind == WALK_KIND_SUB) {
        if (g->subs_out && esha) {
            a_path(mount);
            if (SNIFFFullpath(mount, g->reporoot, path) == OK)
                (void)FILEMakeDirP($path(mount));
            (void)u8bFeed(g->subs_out, path);
            (void)u8bFeed1(g->subs_out, '\t');
            //  20-byte sha → 40-byte hex.
            u8 hex[40];
            u8s hex_s = {hex, hex + 40};
            u8cs bin = {(u8c *)esha, (u8c *)esha + 20};
            (void)HEXu8sFeedSome(hex_s, bin);
            (void)u8bFeed(g->subs_out, ((u8cs){hex, hex + 40}));
            (void)u8bFeed1(g->subs_out, '\n');
        }
        return WALKSKIP;
    }

    //  Sniff-meta paths (.git*, .be*) sometimes leak into legacy
    //  trees but must never be materialised on disk — the live wtlog
    //  / store dir would be clobbered.  Skip both the dir creation
    //  and the file write.  The check is hard-coded to the literal
    //  meta names rather than going through `SNIFFSkipMeta` (which
    //  also applies user-loaded .gitignore patterns and would cause
    //  large gitignores to skip arbitrary tracked subtrees during
    //  cross-tag rollbacks — see git/contrib/* in mill-mother).
    if (!$empty(path)) {
        a_cstr(m_git, ".git");
        DOGa_be(m_be);
        b8 hit = NO;
        $eachseg(seg, path) {
            if (u8csEq(seg, m_git)) { hit = YES; break; }
            if (u8csEq(seg, m_be))  { hit = YES; break; }
        }
        if (hit) return (kind == WALK_KIND_DIR) ? WALKSKIP : OK;
    }

    if (kind == WALK_KIND_DIR) {
        if ($empty(path)) return OK;    // root; walker recurses
        a_path(dp);
        SNIFFFullpath(dp, g->reporoot, path);
        FILEMakeDirP($path(dp));
        return OK;
    }

    //  No-op overlay: target's content at this path equals baseline's;
    //  pre-flight wrote a GET_V_NOOP ULOG row in lockstep with this
    //  walk's order.  Skip the write so dirty user edits aren't
    //  clobbered by a rewrite of identical bytes.  Don't stamp either —
    //  the file's mtime stays whatever the user left it as.
    //
    //  Exception: if the file was wiped from disk (lstat fails), there
    //  is nothing to preserve — fall through and recreate it.
    if (!u8csEmpty(g->noop_cursor)) {
        ulogrec rec = {};
        a_dup(u8c, peek, g->noop_cursor);
        if (ULOGu8sDrain(peek, &rec) == OK
            && u8csEq(rec.uri.path, path)) {
            ulogrec _consume = {};
            (void)ULOGu8sDrain(g->noop_cursor, &_consume);
            a_path(probe);
            if (SNIFFFullpath(probe, g->reporoot, path) == OK) {
                filestat fs = {};
                if (FILELStat(&fs, $path(probe)) == OK) {
                    //  Present on disk — preserve user bytes.  Row's
                    //  verb tells us whether the file was dirty; if
                    //  so, surface `mod` so the kept-untouched edit
                    //  doesn't go silent.
                    if (rec.verb == GET_V_MOD) {
                        ulogrec rep = {.ts = g->ts, .verb = GET_V_MOD};
                        u8csMv(rep.uri.path, path);
                        (void)ROWSPrintRow(&rep, ROWS_NAV_DIFF);
                    }
                    return OK;
                }
            }
            //  File missing → fall through to get_write_one.
        }
    }

    //  Merge path: wt has a real local edit and the merge drain will
    //  weave-merge it against tgt afterwards.  Skip the write here so
    //  the wt's edits stay live for the drain to read.  No restamp
    //  either — the drain stamps after writing the merged bytes.
    if (!u8csEmpty(g->merges_cursor)) {
        ulogrec rec = {};
        a_dup(u8c, peek, g->merges_cursor);
        if (ULOGu8sDrain(peek, &rec) == OK
            && u8csEq(rec.uri.path, path)) {
            ulogrec _consume = {};
            (void)ULOGu8sDrain(g->merges_cursor, &_consume);
            return OK;
        }
    }

    ok64 o = get_write_one(g, path, kind, esha);
    if (o != OK) g->error = o;
    return o;
}

// --- Pre-flight classifier via baseline ↔ target ULOG-row merge.
//
//  Materialise both trees as ULOG-shaped row buffers (KEEPTreeULog),
//  then heap-walk via SNIFFMergeWalk.  Per distinct path the step
//  callback sees a tie group of 1-2 records (one per side).  Decisions:
//    * both sides + identical mode/sha → no-op overlay: WRITE skips
//      so dirty user content survives.  Appended to `noop_out`.
//    * either side is mode 160000 (gitlink) → submodule, ignored.
//    * real change / add / delete  → lstat; if mtime ∉ stamp-set,
//      conflict.  For deletes (baseline-only) where the file is clean
//      and present on disk, append to `unlink_out`.
//    * baseline differs from target AND wt has an unattributed mtime:
//        - if wt content hashes to the baseline sha → clean stamp drift,
//          let checkout overwrite + restamp (no special bucket).
//        - else (real local edit) → append to `merges_out`; checkout
//          skips the path so the merge drain (`GRAFMergeWtFile`) can
//          weave-merge wt-on-disk against tgt afterwards.
//
//  `noop_out`, `unlink_out`, and `merges_out` are reset on entry; on
//  success each carries a ULOG-row stream (one row per affected
//  path, lex-sorted by classifier walk order) with verb
//  GET_V_NOOP / GET_V_DEL / GET_V_MRG respectively and the wt-side
//  baseline-blob hex in `uri.fragment` where available.  Format is
//  the same `<ron60-ts>\t<verb>\t<uri>\n` the wtlog uses, so drains
//  parse via `ULOGu8sDrain`.

typedef struct {
    u8cs   reporoot;
    u8bp   noop_out;
    u8bp   unlink_out;
    u8bp   merges_out;
    ron60  v_base;
    ron60  v_tgt;
    u32    no_base_conflicts;   // dirty wt without a baseline to merge against
    b8     force;               // --force: overwrite dirty paths, no merge
} get_overlap_ctx;

//  Compare two ULOG-row kind (verb's bottom RON64 digit) and
//  uri.fragment (hex sha) by content.  YES iff both kind and sha
//  match.
static b8 get_leaf_eq(ulogreccp a, ulogreccp b) {
    if (ok64Lit(a->verb, 0) != ok64Lit(b->verb, 0)) return NO;
    if (u8csLen(a->uri.fragment) != u8csLen(b->uri.fragment)) return NO;
    return memcmp(a->uri.fragment[0], b->uri.fragment[0],
                  u8csLen(a->uri.fragment)) == 0;
}

static b8 get_is_sub(ulogreccp r) {
    return ok64Lit(r->verb, 0) == RON_s;
}

static ok64 get_overlap_step(ulogreccp recs, u32 n, void *vctx) {
    get_overlap_ctx *c = (get_overlap_ctx *)vctx;
    ulogreccp base = NULL;
    ulogreccp tgt  = NULL;
    for (u32 i = 0; i < n; i++) {
        if (ok64stem(recs[i].verb) == c->v_base) base = &recs[i];
        if (ok64stem(recs[i].verb) == c->v_tgt)  tgt  = &recs[i];
    }
    if (!base && !tgt) return OK;

    //  Path is identical in the tie group — peek from whichever side.
    u8cs path = {};
    u8csMv(path, (base ? base->uri.path : tgt->uri.path));

    //  `.be*`, `.git*` are sniff-meta — they sometimes leak into
    //  committed trees (legacy / accidental put), but they must never
    //  participate in the overlap-dirty classifier: the live wtlog
    //  and store dir would always trip the unstamped-mtime check and
    //  refuse every cross-branch GET.  Treat them as if they weren't
    //  in the tree.  Hard-coded names rather than `SNIFFSkipMeta`
    //  (which also folds in user `.gitignore` patterns and would
    //  silently drop tracked tree paths whose basename matches a
    //  broad pattern like `.*`, e.g. linux's root .gitignore — see
    //  get_visit's matching block).
    {
        a_cstr(m_git, ".git");
        DOGa_be(m_be);
        b8 hit = NO;
        $eachseg(seg, path) {
            if (u8csEq(seg, m_git)) { hit = YES; break; }
            if (u8csEq(seg, m_be))  { hit = YES; break; }
        }
        if (hit) return OK;
    }

    b8 is_sub = (base && get_is_sub(base)) || (tgt && get_is_sub(tgt));
    if (is_sub) return OK;        //  gitlink — sniff doesn't manage

    b8 changed = !base || !tgt || !get_leaf_eq(base, tgt);

    if (!changed && tgt) {
        //  `--force`: skip the noop-overlay optimisation so WRITE
        //  overwrites + restamps every target path.  Resetting to
        //  the current tip (`be get --force '?'`) has base.sha ==
        //  tgt.sha for every file, so without this the noop list
        //  would cover the whole tree and dirty user edits would
        //  silently survive the "reset".
        if (c->force) return OK;

        //  Probe disk: an unattributed mtime means the user has
        //  edited the file since the last stamp.  Target bytes match
        //  baseline so GET has nothing to do, but the user-facing
        //  report should still surface `mod` so the dirty edit
        //  doesn't go silent.  Clean / absent cases get GET_V_NOOP
        //  (internal-only — drives the WRITE-pass skip).
        a_path(fp);
        ron60 verb = GET_V_NOOP;
        if (SNIFFFullpath(fp, c->reporoot, path) == OK) {
            filestat fs = {};
            if (FILELStat(&fs, $path(fp)) == OK
                && !SNIFFAtKnown(fs.mtime))
                verb = GET_V_MOD;
        }
        ulogrec r = {.verb = verb};
        u8csMv(r.uri.path, path);
        u8csMv(r.uri.fragment, tgt->uri.fragment);
        return ULOGu8sFeed(u8bIdle(c->noop_out), &r);
    }
    if (!changed) return OK;

    //  changed && !sub: lstat + stamp-set check.
    a_path(fp);
    if (SNIFFFullpath(fp, c->reporoot, path) != OK) return OK;
    filestat fs = {};
    ok64 lo = FILELStat(&fs, $path(fp));
    if (lo == FILENONE) return OK;    // vanished mid-walk
    //  ENOTDIR == a parent path component is a non-directory (e.g. the
    //  wt has a regular file where the target tree wants a subdir).
    //  The path simply isn't present as a wt file — same as ENOENT for
    //  overlap purposes.  Don't abort the whole checkout here; let the
    //  WRITE pass hit it and report the offending path (get_write_one).
    if (lo == FILENOTDIR) return OK;
    if (lo != OK) return lo;             // permissions etc — propagate
    ron60 mr = fs.mtime;

    //  Unattributed mtime without a baseline to compare against:
    //  refuse — there's no "clean drift" answer possible, and graf
    //  has no history to weave-merge with.  Mirrors the prior
    //  blanket dirty-overlap refusal for this corner.  `--force`
    //  bypasses: WRITE will overwrite the dirty bytes.
    if (!SNIFFAtKnown(mr) && !base) {
        if (c->force) return OK;
        if (c->no_base_conflicts < 5)
            fprintf(stderr, "sniff: dirty overlay %.*s\n",
                    (int)$len(path), (char *)path[0]);
        c->no_base_conflicts++;
        return OK;
    }

    //  Unattributed mtime: hash wt bytes and compare to baseline.
    //  Equal → clean stamp drift, let checkout overwrite + restamp
    //  (silent fall-through) — UNLESS tgt is absent (deletion), in
    //  which case the WRITE pass never visits this path and the
    //  unlink branch below has to do the work.  Different → real
    //  local edit, schedule a weave-merge.  `--force` skips the
    //  hash+merge classification — WRITE overwrites dirty bytes
    //  for the tgt-present case; tgt-absent falls into clean delete.
    if (c->force && !SNIFFAtKnown(mr) && base) {
        if (tgt) return OK;
        //  base && !tgt → drop into the clean-delete arm below.
    } else if (!SNIFFAtKnown(mr) && base) {
        sha1 wt_sha = {};
        b8 hashed = NO;
        if (fs.kind == FILE_KIND_REG) {
            u8bp m = NULL;
            if (FILEMapRO(&m, $path(fp)) == OK && m) {
                u8cs body = {u8bDataHead(m), u8bIdleHead(m)};
                KEEPObjSha(&wt_sha, DOG_OBJ_BLOB, body);
                FILEUnMap(m);
                hashed = YES;
            } else if (fs.size == 0) {
                u8cs empty = {NULL, NULL};
                KEEPObjSha(&wt_sha, DOG_OBJ_BLOB, empty);
                hashed = YES;
            }
        }

        sha1 base_sha = {};
        if (u8csLen(base->uri.fragment) == 40) {
            u8s bin_s = {base_sha.data, base_sha.data + 20};
            a_dup(u8c, hex_dup, base->uri.fragment);
            HEXu8sDrainSome(bin_s, hex_dup);
        }
        if (hashed && memcmp(wt_sha.data, base_sha.data, 20) == 0) {
            //  Clean drift.  If tgt also has the path, the WRITE pass
            //  will overwrite + restamp — silent fall-through.  If tgt
            //  is absent (deletion), fall through to the unlink branch
            //  below; otherwise the file lingers in the wt forever
            //  because nothing visits it again.
            if (tgt) return OK;
            //  base && !tgt → drop into the clean-delete arm below.
        } else {
            //  Real local edit: schedule for weave-merge.  The drain
            //  pass (`get_drain_merges`) will read wt-on-disk after
            //  the checkout pass skips this path.  `uri.fragment`
            //  carries the baseline blob sha so future readers can
            //  do patch-id matching without re-walking the tree.
            ulogrec r = {.verb = GET_V_MRG};
            u8csMv(r.uri.path, path);
            u8csMv(r.uri.fragment, base->uri.fragment);
            return ULOGu8sFeed(u8bIdle(c->merges_out), &r);
        }
    }
    if (base && !tgt) {
        //  Clean delete: schedule unlink.  Baseline blob sha kept in
        //  `uri.fragment` (future patch-id dedup; unused today).
        ulogrec r = {.verb = GET_V_DEL};
        u8csMv(r.uri.path, path);
        u8csMv(r.uri.fragment, base->uri.fragment);
        return ULOGu8sFeed(u8bIdle(c->unlink_out), &r);
    }
    return OK;
}

//  `base_tree` may be NULL — first-checkout / no-baseline case.  Every
//  walked path then comes from the target side only; all three output
//  lists end up empty.
static ok64 get_overlap_check(keeper *k, u8cs reporoot,
                              u8cp base_tree, u8cp tgt_tree,
                              u8bp noop_out, u8bp unlink_out,
                              u8bp merges_out, b8 force) {
    sane(k && tgt_tree && noop_out && unlink_out && merges_out);
    u8bReset(noop_out);
    u8bReset(unlink_out);
    u8bReset(merges_out);

    a_cstr(s_base, "base"); a_dup(u8c, db, s_base);
    a_cstr(s_tgt,  "tgt");  a_dup(u8c, dt, s_tgt);
    ron60 v_base = 0, v_tgt = 0;
    call(RONutf8sDrain, &v_base, db);
    call(RONutf8sDrain, &v_tgt,  dt);

    //  ULOG row buffers, one per side.  Linux's tip has ~80 K
    //  leaves at ~80 B per row ≈ 6 MB; older 1 MB caps NOROOM'd
    //  on big repos.  256 MB cap (mmap'd, COW) covers ~3 M leaves
    //  without paging beyond what's actually written.
    Bu8 bu = {}, tu = {};
    call(u8bMap, bu, 1UL << 28);
    call(u8bMap, tu, 1UL << 28);

    ok64 r = OK;
    if (base_tree) r = KEEPTreeULog(base_tree, 0, v_base, bu);
    if (r == OK)   r = KEEPTreeULog(tgt_tree,  0, v_tgt,  tu);
    if (r != OK) { u8bUnMap(bu); u8bUnMap(tu); return r; }

    a_dup(u8c, view_b, u8bData(bu));
    a_dup(u8c, view_t, u8bData(tu));
    a_pad(u8cs, ins, 2);
    u8cssFeed1(ins_idle, view_b);
    u8cssFeed1(ins_idle, view_t);
    a_dup(u8cs, cursors, u8csbData(ins));

    get_overlap_ctx ctx = {
        .noop_out   = noop_out,
        .unlink_out = unlink_out,
        .merges_out = merges_out,
        .v_base     = v_base,
        .v_tgt      = v_tgt,
        .no_base_conflicts = 0,
        .force      = force,
    };
    u8csMv(ctx.reporoot, reporoot);

    ok64 mr = SNIFFMergeWalk(cursors, get_overlap_step, &ctx);
    u8bUnMap(bu); u8bUnMap(tu);
    if (mr != OK) return mr;

    if (ctx.no_base_conflicts > 0) {
        fprintf(stderr,
                "sniff: GET refused — %u dirty file(s) overlay target "
                "paths and have no baseline to merge against; commit, "
                "stash, or reset before checkout\n",
                ctx.no_base_conflicts);
        return SNIFFOVRL;
    }
    done;
}

// --- --prune sweep --------------------------------------------------
//
//  Post-checkout sweep: every `SNIFFClassify` callback whose step is
//  `CLASS_WT_ONLY` is a path on disk that isn't in the (already-
//  advanced) baseline tree.  Unlink it inline — `SNIFFWtULog` has
//  finished its wt walk by the time callbacks fire, so mutating the
//  fs during dispatch is safe.  Gitignore filtering happens upstream
//  in `SNIFFWtULog → SNIFFSkipMeta`.
typedef struct {
    u8cs reporoot;
    u32  dropped;
} prune_ctx;

static ok64 prune_cb(class_step const *step, void *vctx) {
    sane(step && vctx);
    prune_ctx *c = (prune_ctx *)vctx;
    if (step->kind != CLASS_WT_ONLY) return OK;
    u8cs path = {step->path[0], step->path[1]};
    if ($empty(path)) return OK;
    a_path(fp);
    if (SNIFFFullpath(fp, c->reporoot, path) != OK) return OK;
    if (FILEUnLink($path(fp)) == OK) c->dropped++;
    return OK;
}

// --- --force wt-only tracked-orphan sweep (GET-016 Part 2) ----------
//
//  Post-checkout sweep run under `--force`.  A `CLASS_WT_ONLY` path is
//  on disk but absent from the (already-advanced) baseline tree.  When
//  a drifted wt advances its ref past a deletion whose unlink was lost
//  (e.g. a read-only parent dir swallowed the original drain), the
//  orphaned path falls out of every later baseline↔target delta —
//  neither side carries it — so plain GET never revisits it and the
//  stale file lingers forever.  `--force` is the full-reset, so it
//  must converge the wt to the target by dropping such orphans.
//
//  Unlike `--prune` (which removes EVERY wt-only path, sparing only
//  gitignored), force preserves genuinely-untracked files: GET.mkd
//  §Flags promises "untracked files survive unless pruned".  The
//  discriminator is the wt scan row's mtime — a tracked file carries
//  the stamp of the get/post that wrote it (still resident in the
//  append-only wtlog), so `SNIFFAtKnown` recognises it; a never-tracked
//  clutter file has an unstamped mtime and is left in place.
static ok64 force_orphan_cb(class_step const *step, void *vctx) {
    sane(step && vctx);
    prune_ctx *c = (prune_ctx *)vctx;
    if (step->kind != CLASS_WT_ONLY) return OK;
    //  Tracked-only: the wt scan row's mtime must stamp a wtlog row,
    //  marking the file as sniff-written (a prior get/post).  No row
    //  (NULL) or an unstamped mtime → untracked clutter, preserve it.
    if (step->wt_rec == NULL) return OK;
    if (!SNIFFAtKnown(step->wt_rec->ts)) return OK;
    u8cs path = {step->path[0], step->path[1]};
    if ($empty(path)) return OK;
    a_path(fp);
    if (SNIFFFullpath(fp, c->reporoot, path) != OK) return OK;
    if (FILEUnLink($path(fp)) == OK) {
        c->dropped++;
        ulogrec rep = {.verb = GET_V_DEL};
        u8csMv(rep.uri.path, path);
        (void)ROWSPrintRow(&rep, ROWS_NAV_CAT);
    }
    return OK;
}

//  Drain a ULOG-row buffer (GET_V_DEL rows) and unlink each entry.
//  Paths came from get_overlap_check's classifier, which already
//  verified presence + clean stamp; defensive lstat is omitted.
//  After unlinking we walk back up each path's parent chain and
//  `rmdir` every directory that became empty — git's checkout
//  collapses empty dirs the same way, and rsync flags surviving
//  ones as `*deleting <dir>/`.  Reports the file count to stderr.
static ok64 get_drain_unlinks(u8cs reporoot, u8cs unlinks, ron60 ts) {
    sane($ok(reporoot));
    u32 dropped = 0;
    a_dup(u8c, scan, unlinks);
    for (;;) {
        ulogrec rec = {};
        ok64 dr = ULOGu8sDrain(scan, &rec);
        if (dr == NODATA) break;
        if (dr != OK) continue;                  // malformed row — skip
        u8cs path = {rec.uri.path[0], rec.uri.path[1]};
        if ($empty(path)) continue;
        a_path(fp);
        if (SNIFFFullpath(fp, reporoot, path) != OK) continue;
        if (FILEUnLink($path(fp)) == OK) {
            dropped++;
            //  Report row is path-only: the drained row carries the
            //  baseline blob sha in its fragment for future patch-id
            //  dedup, but user-facing status mustn't leak hashes.
            ulogrec rep = {.ts = ts, .verb = GET_V_DEL};
            u8csMv(rep.uri.path, path);
            (void)ROWSPrintRow(&rep, ROWS_NAV_CAT);
        }
        //  Walk up: rmdir any newly-empty parent until we hit a
        //  non-empty dir, the reporoot, or rmdir fails (ENOTEMPTY).
        //  We mutate `fp` in place by truncating at the last `/`.
        u8 *base_p = u8bDataHead(fp);
        u8 *end_p  = u8bIdleHead(fp);
        a_dup(u8c, root_s, reporoot);
        size_t root_len = (size_t)$len(root_s);
        for (;;) {
            //  Trim trailing path component including its slash.
            u8 *slash = NULL;
            for (u8 *p = base_p; p < end_p; p++)
                if (*p == '/') slash = p;
            if (!slash) break;
            //  Don't try to rmdir at-or-above reporoot.
            size_t prefix_len = (size_t)(slash - base_p);
            if (prefix_len <= root_len) break;
            *slash = 0;
            ((u8 **)fp)[2] = slash;       // idle = slash
            if (rmdir((char const *)base_p) != 0) break;
            end_p = slash;
        }
    }
    if (dropped > 0)
        fprintf(stderr, "sniff: pruned %u file(s)\n", dropped);
    done;
}

//  Drain a ULOG-row buffer (GET_V_MRG rows) of weave-merge targets.
//  For each row, call `GRAFMergeWtFile` (reads the live wt file as
//  the implicit edit on `base` and merges against `tgt`'s history),
//  write the merged bytes back to the wt, and stamp the new mtime.
//  `base`/`tgt` are *commit* shas (shared across rows); per-row
//  baseline blob sha lives in `rec.uri.fragment` but is unused today.
//  Best-effort per path: a per-path failure is logged and the rest
//  continue.
static ok64 get_drain_merges(u8cs reporoot, u8cs merges,
                             sha1cp base, sha1cp tgt,
                             ron60 stamp_ts) {
    sane($ok(reporoot) && base && tgt);
    if (u8csEmpty(merges)) done;

    a_carve(u8, out, 1UL << 24);

    u32 merged = 0, failed = 0;
    a_dup(u8c, scan, merges);
    for (;;) {
        ulogrec rec = {};
        ok64 dr = ULOGu8sDrain(scan, &rec);
        if (dr == NODATA) break;
        if (dr != OK) continue;                  // malformed — skip
        u8cs path = {rec.uri.path[0], rec.uri.path[1]};
        if ($empty(path)) continue;

        ok64 mr = GRAFMergeWtFile(path, reporoot, base, tgt, out);
        if (mr != OK) {
            fprintf(stderr,
                    "sniff: merge failed for %.*s (graf err) — "
                    "leaving wt content untouched\n",
                    (int)$len(path), (char *)path[0]);
            failed++;
            continue;
        }

        a_path(fp);
        if (SNIFFFullpath(fp, reporoot, path) != OK) {
            failed++; continue;
        }
        int fd = -1;
        if (FILECreate(&fd, $path(fp)) != OK) {
            failed++; continue;
        }
        u8cs body = {u8bDataHead(out), u8bIdleHead(out)};
        ok64 wo = FILEFeedAll(fd, body);
        FILEClose(&fd);
        if (wo != OK) { failed++; continue; }

        //  Stamp at ts+1 (one ron60 ms tick past the GET row) so the
        //  merged bytes land outside the stamp-set and `be` flags them
        //  as user-dirty — the merge result is a user edit, not clean
        //  baseline.
        (void)SNIFFAtStampPath(fp, stamp_ts + 1);
        merged++;
        //  Same path-only report shape as the unlink drain.
        ulogrec rep = {.ts = stamp_ts, .verb = GET_V_MRG};
        u8csMv(rep.uri.path, path);
        (void)ROWSPrintRow(&rep, ROWS_NAV_DIFF);
    }

    if (merged > 0)
        fprintf(stderr, "sniff: weave-merged %u file(s)\n", merged);
    if (failed > 0)
        fprintf(stderr, "sniff: merge failures: %u file(s)\n", failed);
    done;
}

// --- Pre-flight: cross-branch wt-dirty scan ---
//
//  Walks every non-meta file in the wt; if any has an mtime that
//  isn't in sniff's stamp-set, the wt is considered dirty and a
//  cross-branch GET must refuse.  Same membership rule as
//  `sniff status` (shared via SNIFFAtScanDirty).

static ok64 get_wt_dirty_cb(u8cs rel, void *ctx) {
    sane(ctx);
    u32 *count = (u32 *)ctx;
    if (*count < 5)
        fprintf(stderr, "sniff: dirty %.*s\n",
                (int)$len(rel), (char *)rel[0]);
    (*count)++;
    return OK;
}

static b8 get_wt_dirty(u8cs reporoot) {
    u32 count = 0;
    (void)SNIFFAtScanDirty(reporoot, get_wt_dirty_cb, &count);
    return count > 0;
}

//  Hole verifier — walk the reachable closure of `target_hex` through
//  the keeper's current PastData view and report (`stderr`) any object
//  that's locally absent.  Returns OK iff every reachable
//  commit/tree/blob is present; KEEPFAIL otherwise.
//
//  Use case: `--force` on `be get` activates this BEFORE any state
//  change (no keeper switch, no wt write).  The current keeper context
//  is the wt's cur leaf — PAST already covers trunk + every loaded
//  ancestor branch, so cross-shard targets verify without switching
//  (KEEPSwitchBranch would itself fail when the target shard is
//  broken, defeating the purpose).  Refusing the checkout up-front
//  preserves a working ?onto-remote while exposing what's missing
//  upstream.
//
//  Target must be a full 40-char hex (KEEPVerify's contract).
static ok64 get_verify_closure(keeper *k, u8csc target_hex) {
    sane(k && $ok(target_hex));
    if ($len(target_hex) != 40) {
        fprintf(stderr,
            "sniff: --force verify: target must be 40-hex (got %zu)\n",
            (size_t)$len(target_hex));
        return SNIFFFAIL;
    }
    a_dup(u8c, hex_mut, target_hex);
    ok64 vo = KEEPVerify(hex_mut);
    if (vo != OK) {
        fprintf(stderr,
            "sniff: --force verify: target %.*s has missing objects "
            "(see `keeper verify '#%.*s'` for the list).  "
            "Refusing checkout — repair the local store first "
            "(e.g. re-fetch from a peer that has the closure).\n",
            (int)$len(target_hex), (char *)target_hex[0],
            (int)$len(target_hex), (char *)target_hex[0]);
        return SNIFFFAIL;
    }
    return OK;
}

// --- FF-only check for branch-named GET ---
//
//  https://replicated.wiki/html/wiki/GET.html §GET: when GET advances a local branch to a new tip
//  (`be get ssh://origin?A` or its cached `//origin?A` form), the
//  local tip must be an ancestor of (or equal to) the incoming
//  tip.  Refuse SNIFFNOFF on divergence; user reconciles with
//  `be patch //origin?A#` + `be post`.  Mirrors the FF check at
//  sniff/POST.c §"ff-only pre-flight": LCA(a, b) == a iff a is an
//  ancestor of b.  Graf opened RO inline (idempotent, same pattern
//  as `get_drain_merges`).
static ok64 get_ff_check(sha1cp local_tip, sha1cp target_tip) {
    sane(local_tip && target_tip);
    if (sha1Eq(local_tip, target_tip)) done;

    ok64 go = GRAFOpen(NO);
    if (go != OK && go != GRAFOPEN && go != GRAFOPENRO) return go;
    b8 own_open = (go == OK);

    sha1 lca = {};
    (void)GRAFLca(&lca, local_tip, target_tip);
    if (own_open) GRAFClose();

    if (sha1Eq(&lca, local_tip)) done;          //  local is ancestor — FF
    return SNIFFNOFF;
}

//  Resolve the local-only tip of `?<branch>` by querying REFS with a
//  no-authority URI.  Returns OK + fills `*out` on hit; NONE on miss.
//  Branch slice excludes the leading `?` (callers pass the body).
static ok64 get_local_branch_tip(sha1 *out, u8cs branch) {
    sane(out && $ok(branch));
    keeper *k = &KEEP;
    if (u8csEmpty(branch)) return NONE;
    //  40-hex `branch` means caller really passed a detached sha; nothing
    //  to advance, no FF check applies.
    if (DOGIsFullSha(branch)) return NONE;

    //  Look up the local-only tip via bare `?<branch>` (the shape
    //  sniff POST writes — peer-keyed rows from wire fetches use
    //  `<peer-uri>?<branch>` and aren't a "local tip" for FF purposes).
    //  REFS lives in the active leaf branch dir (sub-shard isolation:
    //  a sub mount has its OWN REFS at `<root>/.be/<leaf>/refs`, not
    //  the parent's trunk).  Trunk REFS holds the primary wt's own
    //  rows when leaf is empty.
    a_path(keepdir);
    call(HOMEBranchDir, keepdir, HOME.cur_branch);
    a_pad(u8, refkey_buf, 256);
    u8bFeed1(refkey_buf, '?');
    u8bFeed(refkey_buf, branch);
    a_dup(u8c, refkey, u8bData(refkey_buf));

    a_pad(u8, arena, 1024);
    uri resolved = {};
    ok64 ro = REFSResolve(&resolved, arena, $path(keepdir), refkey);
    if (ro != OK || $empty(resolved.query)) return NONE;

    u8cs tip_hex = {resolved.query[0], resolved.query[1]};
    if (!u8csEmpty(tip_hex) && *tip_hex[0] == '?') u8csUsed(tip_hex, 1);
    if ($len(tip_hex) != 40) return NONE;

    if (sha1FromHex(out, tip_hex) != OK) return NONE;
    done;
}

// --- Commit-range banner ------------------------------------------------
//
//  Before WALKTreeLazy lays down per-file rows, summarise the commit
//  range this checkout traverses: walk each tip's ancestor closure
//  through graf's DAG and emit one ULOG row per commit in the symmetric
//  difference as
//      <ts>\tpost\t?<hashlet8>#<subject>\n
//  Disapplied (base \ tgt) commits come out first newest-first; applied
//  (tgt \ base) commits follow newest-first.  Both directions use verb
//  `post` — the wt's narrative reads top-to-bottom as "rolling back …
//  applying …".
//
//  Best-effort: missing DAG runs / commit body / graf open quietly skip
//  the banner — the checkout proceeds and per-file rows still emit.

#define GET_CRANGE_ANC_CAP   (1u << 16)
#define GET_CRANGE_OBJ_BUF   (1UL << 20)
#define GET_CRANGE_SUBJ_MAX  120

static void get_emit_one_commit(u64 h40, ron60 ts, Bu8 cbuf) {
    u8bReset(cbuf);
    u8 ot = 0;
    if (KEEPGet(h40, DAG_H60_HEXLEN, cbuf, &ot) != OK ||
        ot != DOG_OBJ_COMMIT) return;
    a_dup(u8c, body, u8bData(cbuf));
    sha1 csha = {};
    KEEPObjSha(&csha, DOG_OBJ_COMMIT, body);

    //  Walk headers: grab the author header's ts (so each row carries
    //  the commit's own wall-clock time, not the GET command's now),
    //  then the empty-field sentinel hands us the message body.
    u8cs  message  = {};
    ron60 ctime_ts = 0;
    {
        a_dup(u8c, scan, body);
        u8cs field = {}, value = {};
        while (GITu8sDrainCommit(scan, field, value) == OK) {
            if (u8csEmpty(field)) { $mv(message, value); break; }
            if (ctime_ts == 0 && u8csEq(field, GIT_FIELD_AUTHOR)) {
                u8csc ident = {value[0], value[1]};
                u8cs  nm = {}, em = {};
                GITu8sIdent(ident, nm, em, &ctime_ts);
            }
        }
    }
    //  Trim leading blank lines, clip to the first line; treat TAB as
    //  a row terminator too so a stray tab in the subject can't break
    //  ULOG's field separator.  Cap to keep the row terminal-friendly.
    u8cp ms = message[0];
    u8cp mend = message[1];
    while (ms < mend && (*ms == '\n' || *ms == '\r')) ms++;
    u8cp me = ms;
    while (me < mend && *me != '\n' && *me != '\r' && *me != '\t') me++;
    if ((size_t)(me - ms) > GET_CRANGE_SUBJ_MAX)
        me = ms + GET_CRANGE_SUBJ_MAX;

    a_pad(u8, qbuf, SHA1_HASHLEN_LEN);
    if (SHA1u8sFeedHashlet(qbuf_idle, &csha) != OK) return;

    ulogrec rep = {.ts = ctime_ts ? ctime_ts : ts,
                   .verb = SNIFFAtVerbPost()};
    a_dup(u8c, q, u8bData(qbuf));
    u8csMv(rep.uri.query, q);
    rep.uri.fragment[0] = ms;
    rep.uri.fragment[1] = me;
    (void)ROWSPrintRow(&rep, ROWS_NAV_COMMIT);
}

static void get_emit_commit_list(sha1cp base_commit, sha1cp tgt_commit,
                                 ron60 ts) {
    if (!base_commit || !tgt_commit) return;
    if (sha1Eq(base_commit, tgt_commit)) return;

    ok64 go = GRAFOpen(NO);
    b8 own_open = (go == OK);
    if (go != OK && go != GRAFOPEN && go != GRAFOPENRO) return;

    wh128css runs = {NULL, NULL};
    GRAFRuns(runs);

    Bwh128 anc_base = {}, anc_tgt = {};
    Bu8    ord_base = {}, ord_tgt = {}, cbuf = {};
    do {
        if (wh128bAllocate(anc_base, GET_CRANGE_ANC_CAP) != OK) break;
        if (wh128bAllocate(anc_tgt,  GET_CRANGE_ANC_CAP) != OK) break;
        if (u8bMap(ord_base, GET_CRANGE_ANC_CAP * sizeof(u64)) != OK) break;
        if (u8bMap(ord_tgt,  GET_CRANGE_ANC_CAP * sizeof(u64)) != OK) break;
        if (u8bMap(cbuf,     GET_CRANGE_OBJ_BUF) != OK) break;

        u64 base_h = WHIFFHashlet60(base_commit);
        u64 tgt_h  = WHIFFHashlet60(tgt_commit);
        DAGAncestors(anc_base, runs, base_h);
        DAGAncestors(anc_tgt,  runs, tgt_h);

        u64 *ob = (u64 *)u8bDataHead(ord_base);
        u64 *ot = (u64 *)u8bDataHead(ord_tgt);
        u32  nb = DAGTopoSort(ob, GET_CRANGE_ANC_CAP, anc_base, runs);
        u32  nt = DAGTopoSort(ot, GET_CRANGE_ANC_CAP, anc_tgt,  runs);

        //  Disapplied (base \ tgt), newest-first.
        for (u32 i = nb; i > 0; i--) {
            u64 h = ob[i - 1];
            if (h == 0) continue;
            if (DAGAncestorsHas(anc_tgt, h)) continue;
            get_emit_one_commit(h, ts, cbuf);
        }
        //  Applied (tgt \ base), newest-first.
        for (u32 i = nt; i > 0; i--) {
            u64 h = ot[i - 1];
            if (h == 0) continue;
            if (DAGAncestorsHas(anc_base, h)) continue;
            get_emit_one_commit(h, ts, cbuf);
        }
    } while (0);

    //  u8bUnMap / wh128bFree on a zero-init buf return FAILSANITY /
    //  BISNULL — harmless, so the partial-allocation cleanup is
    //  unconditional.
    u8bUnMap(cbuf);
    u8bUnMap(ord_tgt);
    u8bUnMap(ord_base);
    if (wh128bHead(anc_tgt)  != wh128bTerm(anc_tgt))  wh128bFree(anc_tgt);
    if (wh128bHead(anc_base) != wh128bTerm(anc_base)) wh128bFree(anc_base);
    if (own_open) GRAFClose();
}

// --- Public API ---

ok64 GETCheckout(u8cs reporoot, u8csc hex, u8csc source) {
    sane($ok(hex));
    keeper *k = &KEEP;

    //  --force escape hatch: verify the target's closure up-front
    //  (no state change) and refuse the checkout when bytes are
    //  missing.  Runs from the wt's current cur context so a broken
    //  target shard doesn't trip the KEEPSwitchBranch path that would
    //  otherwise hide the holes.
    u8cs hexs = {hex[0], hex[1]};
    if (SNIFF.force && DOGIsFullSha(hexs)) {
        call(get_verify_closure, k, hex);
    }

    size_t hexlen = $len(hex);
    if (hexlen > 15) hexlen = 15;
    u64 hashlet = WHIFFHexHashlet60(hex);

    //  Cross-branch GET pre-switch: when `source` names a different
    //  branch than the wt's current baseline, slide keeper's DATA
    //  (cur's packs) into PAST and load the target branch's packs
    //  into DATA via `SNIFFMaybeSwitchKeeper`.  Has to happen BEFORE
    //  the KEEPGet below — the target's tip commit lives in the
    //  target branch's pack, which `KEEPOpenBranch(cur)` did not
    //  register.  See KEEP.h §KEEPSwitchBranch.
    {
        u8cs t_branch = {};
        if ($ok(source) && !u8csEmpty(source) &&
            *source[0] == '?' && $len(source) != 41) {
            t_branch[0] = $atp(source, 1);
            t_branch[1] = source[1];
        }
        if (!u8csEmpty(t_branch)) {
            //  URI-001 Stage 4b: the branch portion IS the whole
            //  `t_branch` slice — the located `?<branch>/<sha>` form is
            //  retired, and the `$len(source) != 41` gate above already
            //  excludes a bare detached full-sha.  SNIFFMaybeSwitch*
            //  probes `<root>/.be/<branch>/` and no-ops on a non-branch,
            //  so no string heuristic is needed here.
            u8cs to_switch = {};
            u8csMv(to_switch, t_branch);
            //  Graf reads h->cur_branch as its "from" — order before keeper.
            //  Graf-side switch is best-effort: a graf miss only affects
            //  history-walk side-effects, not the KEEPGet below; keeper
            //  is the one we hard-check.
            try(SNIFFMaybeSwitchGraf, to_switch);
            __ = OK;
            ok64 so = SNIFFMaybeSwitchKeeper(to_switch);
            if (so != OK && so != KEEPOPEN) {
                fprintf(stderr,
                        "sniff: cannot switch keeper to ?%.*s: %s\n",
                        (int)$len(to_switch),
                        (char *)to_switch[0],
                        ok64str(so));
                fail(SNIFFFAIL);
            }
        } else if ($ok(source) && !u8csEmpty(source) &&
                   *source[0] == '?' && $len(source) == 1) {
            //  Bare `?` (trunk) — switch back to trunk-leaf view.
            //  Pass an "empty but valid" slice so KEEPSwitchBranch's
            //  `$ok(branch)` holds.
            if (!u8csEmpty(u8bDataC(HOME.cur_branch))) {
                static u8c const _zero = 0;
                u8cs empty = {(u8cp)&_zero, (u8cp)&_zero};
                ok64 so = KEEPSwitchBranch(empty);
                if (so != OK && so != KEEPOPEN) {
                    fprintf(stderr,
                            "sniff: cannot switch keeper to trunk: %s\n",
                            ok64str(so));
                    fail(SNIFFFAIL);
                }
            }
        }
    }

    a_carve(u8, buf, 1UL << 24);
    u8 otype = 0;
    ok64 o = KEEPGet(hashlet, hexlen, buf, &otype);
    if (o != OK) {
        a_path(leafdir);
        (void)HOMEBranchDir(leafdir, HOME.cur_branch);
        fprintf(stderr,
                "sniff: object not found: hex=" U8SFMT
                " hexlen=%zu hashlet=%016llx source=" U8SFMT
                " root=" U8SFMT " project=" U8SFMT
                " cur_branch=" U8SFMT " leafdir=" U8SFMT
                " packs=%zu keep_err=%s\n",
                u8sFmt(hex), (size_t)hexlen,
                (unsigned long long)hashlet,
                u8sFmt(source),
                u8sFmt(u8bDataC(HOME.root)),
                u8sFmt(u8bDataC(HOME.project)),
                u8sFmt(u8bDataC(HOME.cur_branch)),
                u8sFmt(u8bDataC(leafdir)),
                (size_t)$len(kv64bData(KEEP.packs)),
                ok64str(o));
        fail(SNIFFFAIL);
    }

    //  Dereference annotated tag.
    if (otype == DOG_OBJ_TAG) {
        u8cs body = {u8bDataHead(buf), u8bIdleHead(buf)};
        u8cs field = {}, value = {};
        sha1 tag_sha = {};
        a_raw(tag_bin, tag_sha);
        b8 found = NO;
        while (GITu8sDrainCommit(body, field, value) == OK) {
            if ($empty(field)) break;
            if ($len(field) == 6 && memcmp(field[0], "object", 6) == 0 &&
                $len(value) >= 40) {
                u8cs hex40 = {value[0], $atp(value, 40)};
                HEXu8sDrainSome(tag_bin, hex40);
                found = YES;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "sniff: bad tag (no object)\n");
            fail(SNIFFFAIL);
        }
        u8bReset(buf);
        o = KEEPGetExact(&tag_sha, buf, &otype);
        if (o != OK || otype != DOG_OBJ_COMMIT) {
            fprintf(stderr, "sniff: tag target not a commit\n");
            fail(SNIFFFAIL);
        }
    }

    if (otype != DOG_OBJ_COMMIT) {
        fprintf(stderr, "sniff: not a commit\n");
        fail(SNIFFFAIL);
    }

    sha1 tree_sha = {}, tgt_commit_sha = {};
    u8cs commit = {u8bDataHead(buf), u8bIdleHead(buf)};
    o = GITu8sCommitTree(commit, tree_sha.data);
    //  Hash the commit body for the weave-merge drain (its sha is the
    //  tgt commit hashlet graf walks from).
    KEEPObjSha(&tgt_commit_sha, DOG_OBJ_COMMIT, commit);
    if (o != OK) {
        fprintf(stderr, "sniff: bad commit (no tree)\n");
        fail(SNIFFFAIL);
    }

    //  --- FF-only gate for branch-named GET ---------------------
    //  https://replicated.wiki/html/wiki/GET.html §GET: `be get ssh://origin?A` is fast-forward only
    //  on the local tip of branch A.  Local switches (`be get ?feat`)
    //  resolve target == local-tip so this check is a no-op; the
    //  walk only runs when a remote-tracking row advances `?A` past
    //  the local tip on a divergent history.  --force overrides
    //  (matches the dirty-overlay escape hatch).
    if (!SNIFF.force && $ok(source) && !u8csEmpty(source) &&
        *source[0] == '?' && $len(source) != 41 && $len(source) > 1) {
        u8cs branch = {$atp(source, 1), source[1]};
        sha1 local_tip = {};
        if (get_local_branch_tip(&local_tip, branch) == OK) {
            ok64 ffo = get_ff_check(&local_tip, &tgt_commit_sha);
            if (ffo == SNIFFNOFF) {
                fprintf(stderr,
                        "sniff: get: ?%.*s — not a fast-forward "
                        "(local tip is not an ancestor of incoming); "
                        "reconcile with `be patch //...?%.*s#` + "
                        "`be post`, or pass --force to overwrite\n",
                        (int)$len(branch), (char *)branch[0],
                        (int)$len(branch), (char *)branch[0]);
                return SNIFFNOFF;
            }
            //  Any other walk error (out-of-memory etc.) — propagate.
            if (ffo != OK) return ffo;
        }
    }

    //  --- Pre-flight gate: cross-branch wt-dirty refuse ---------
    //  Compare baseline branch (latest get/post/patch row) to the
    //  target branch.  When they differ, any unattributed mtime in
    //  the wt blocks the switch — the caller must commit, stash,
    //  or reset before `be get` will move them off the current
    //  branch.  Same-branch GETs (refresh, restore wiped wt, or
    //  switch tips on the same branch) skip this gate; the per-
    //  file overlap pre-flight further down is sufficient there.
    {
        ron60 bts = 0, bverb = 0;
        uri bu = {};
        if (SNIFFAtCurTip(&bts, &bverb, &bu) == OK) {
            u8cs t_branch = {};
            if ($ok(source) && !u8csEmpty(source) &&
                *source[0] == '?' && $len(source) != 41) {
                t_branch[0] = $atp(source, 1);
                t_branch[1] = source[1];
            }
            u8cs b_branch = {bu.query[0], bu.query[1]};
            //  DIS-009: a detached baseline row is `?<40hex>` — the
            //  query slot holds a sha, not a branch.  The target side
            //  already drops a detached `?<sha>` (len 41) to an empty
            //  branch; mirror that here so detached→detached overlays
            //  (`be get <sha>` then `be get <other-sha>`) are NOT seen
            //  as a cross-branch switch and the dirty gate stays open
            //  for the weave-merge path.
            if (DOGIsFullSha(b_branch)) {
                b_branch[0] = NULL;
                b_branch[1] = NULL;
            }
            b8 same_branch = u8csEq(b_branch, t_branch);

            if (!same_branch && !SNIFF.force && get_wt_dirty(reporoot)) {
                fprintf(stderr,
                        "sniff: cross-branch GET refused "
                        "— wt is dirty (use --force to override)\n");
                fail(SNIFFDRTY);
            }
            //  KEEPSwitchBranch already ran at the top of this fn —
            //  reads below operate on the post-switch view.
        }
    }
    //  --- end pre-flight gate ------------------------------------

    //  Resolve the baseline tree from the latest get/post/patch row.
    //  Used by the overlap pre-flight to distinguish "incoming change"
    //  from "no-op overlay".  Absent baseline (fresh wt) → NULL pointer
    //  passes through, every target path is treated as incoming.
    sha1 base_tree = {}, base_commit_sha = {};
    b8 has_base_tree = NO, has_base_commit = NO;
    {
        ron60 bts = 0, bverb = 0;
        uri bu = {};
        if (SNIFFAtCurTip(&bts, &bverb, &bu) == OK) {
            sha1hex hex = {};
            if (SNIFFAtQueryFirstSha(&bu, &hex) == OK) {
                //  Decode the baseline tip hex into a sha1 for the
                //  weave-merge drain (see get_drain_merges).
                if (sha1FromSha1hex(&base_commit_sha, &hex) == OK)
                    has_base_commit = YES;

                u8cs hex_s = {};
                sha1hexSlice(hex_s, &hex);
                u64 bhashlet = WHIFFHexHashlet60(hex_s);
                a_carve(u8, cbuf, 1UL << 24);
                {
                    u8 ctype = 0;
                    if (KEEPGet(bhashlet, 40, cbuf, &ctype) == OK) {
                        //  Peel annotated tag → commit (mill-tags
                        //  baselines like `?tags/v2.52.0` resolve
                        //  to a TAG object, not the commit it
                        //  points at).  Mirrors KEEPResolveTree.
                        if (ctype == DOG_OBJ_TAG) {
                            a_dup(u8c, tbody, u8bData(cbuf));
                            u8cs tf = {}, tv = {};
                            sha1 tag_target = {};
                            b8 got = NO;
                            while (GITu8sDrainCommit(tbody, tf, tv) == OK) {
                                if (u8csEmpty(tf)) break;
                                if (u8csLen(tf) == 6 &&
                                    memcmp(tf[0], "object", 6) == 0 &&
                                    u8csLen(tv) >= 40) {
                                    u8s sb = {tag_target.data,
                                              tag_target.data + 20};
                                    u8cs hx = {tv[0], tv[0] + 40};
                                    if (HEXu8sDrainSome(sb, hx) == OK)
                                        got = YES;
                                    break;
                                }
                            }
                            if (got) {
                                u64 ch = WHIFFHashlet60(&tag_target);
                                u8bReset(cbuf);
                                ctype = 0;
                                (void)KEEPGet(ch, 40, cbuf, &ctype);
                                //  Override base_commit_sha with the
                                //  peeled commit so weave-merge sees
                                //  the right history root.
                                sha1Mv(&base_commit_sha, &tag_target);
                            }
                        }
                        if (ctype == DOG_OBJ_COMMIT) {
                            u8cs cbody = {u8bDataHead(cbuf),
                                          u8bIdleHead(cbuf)};
                            if (GITu8sCommitTree(cbody, base_tree.data) == OK)
                                has_base_tree = YES;
                        }
                    }
                }
            }
        }
    }

    //  256 MB mmap'd (COW, pay-on-write) — same envelope as the ULOG
    //  row buffers inside `get_overlap_check`.  The previous 1 MB
    //  heap allocation truncated under big tag-to-tag deltas (linux's
    //  v7.x → v6.1.y rollback drops ~14 K paths whose names sort late
    //  in lex order — `tools/v*`, `tools/w*` — those tail entries
    //  silently lost their `u8bFeed` and never reached the unlink
    //  drainer, leaving stale files on disk after checkout).
    Bu8 noop = {}, unlinks = {}, merges = {}, blob = {}, subs = {};
    //  Cumulative cleanup on partial-allocation failure: u8bUnMap /
    //  u8bFree on a zero-init Bu8 return FAILSANITY / BISNULL, harmless.
#define GET_BUFS_FREE()                                                   \
    do { u8bUnMap(noop); u8bUnMap(unlinks); u8bUnMap(merges);             \
         u8bUnMap(blob); u8bFree(subs); } while (0)
    {
        ok64 ao;
        if ((ao = u8bMap(noop,    1UL << 28)) != OK) { GET_BUFS_FREE(); return ao; }
        if ((ao = u8bMap(unlinks, 1UL << 28)) != OK) { GET_BUFS_FREE(); return ao; }
        if ((ao = u8bMap(merges,  1UL << 28)) != OK) { GET_BUFS_FREE(); return ao; }
        //  Per-file blob scratch for the WRITE pass (get_ctx.blob).
        if ((ao = u8bMap(blob,    1UL << 28)) != OK) { GET_BUFS_FREE(); return ao; }
        //  Submodule (`160000`) collector — kept small; most trees have
        //  O(10) gitlinks at most.  64 KB ≫ any realistic count.
        if ((ao = u8bAllocate(subs, 1UL << 16)) != OK) { GET_BUFS_FREE(); return ao; }
    }
    //  TODO: even on a fresh clone (`base_tree==NULL`) the overlap
    //  check still walks the target tree once to detect dirty wt
    //  files sitting at target-tree paths (the
    //  `no_base_conflicts` refusal — see be-get-overlay-no-baseline
    //  test).  That makes sniff GET walk the target tree TWICE on
    //  a fresh clone (once here, once in WALKTreeLazy below).  A
    //  better split would fuse the dirty-overlap detection into
    //  the WRITE pass and skip this pre-flight when there's no
    //  baseline to compare against; left as an optimisation since
    //  the safety check has to run somewhere.
    o = get_overlap_check(k, reporoot,
                          has_base_tree ? base_tree.data : NULL,
                          tree_sha.data, noop, unlinks, merges,
                          SNIFF.force);
    if (o != OK) {
        GET_BUFS_FREE();
        return o;
    }

    get_ctx ctx = {.k = k, .error = OK};
    u8csMv(ctx.reporoot, reporoot);
    SNIFFAtNow(&ctx.ts, &ctx.tv);
    ctx.noop_cursor[0]   = u8bDataHead(noop);
    ctx.noop_cursor[1]   = u8bIdleHead(noop);
    ctx.merges_cursor[0] = u8bDataHead(merges);
    ctx.merges_cursor[1] = u8bIdleHead(merges);
    ctx.subs_out         = subs;
    ctx.blob             = blob;

    //  --- COMMIT POINT (atomicity) -------------------------------
    //  Append the `get` ULOG row and advance the local branch tip
    //  BEFORE we mutate the worktree.  Rationale: the wt-mutation
    //  block below (WALKTreeLazy + drains + submodule loop) is not
    //  itself crash-safe — a partial run leaves a mix of new
    //  contents and old paths.  By recording the new baseline first
    //  we preserve git's invariant: REFS + ULOG always agree; the
    //  wt may drift but is always reconcilable by replaying the
    //  same GET (`be get --force ?<cur>` after a crash).  Stamping
    //  uses ctx.ts/ctx.tv, the SAME ts as this row — files written
    //  below round-trip into the stamp-set on the next status.
    //
    //  The pre-flight gates above (cross-branch dirty refuse, FF
    //  check, overlap check) have already passed; nothing below
    //  can refuse — only fail partway, which is what the reorder
    //  is designed to make survivable.
    {
        uri urow = {};
        a_pad(u8, qbuf, 128);
        //  Row-shape decision (AT.md §"Row vocabulary"):
        //    * detached `?<40hex>` (source len 41) → keep the sha in the
        //      QUERY, leave the fragment EMPTY: `?<sha>`.  Turning this
        //      into `?#<sha>` would alias trunk-state (attached to trunk
        //      at that sha) and let POST/PATCH silently commit — see
        //      DIS-009.  Detached must stay detached.
        //    * attached branch `?<branch>` (len != 41) → branch in the
        //      QUERY, tip sha in the FRAGMENT: `?<branch>#<sha>`.
        //    * empty source (trunk) → empty query, sha in fragment:
        //      `?#<sha>` (trunk-state; POST commits back to trunk).
        b8 detached = $ok(source) && !u8csEmpty(source) &&
                      *source[0] == '?' && $len(source) == 41;
        if ($ok(source) && !u8csEmpty(source) && *source[0] == '?' &&
            $len(source) != 41) {
            a_dup(u8c, q, source);
            u8csUsed1(q);
            u8bFeed(qbuf, q);
        }
        if (detached) {
            //  sha → query slot, fragment stays empty.
            a_dup(u8c, h, hex);
            urow.query[0] = h[0];
            urow.query[1] = h[1];
        } else {
            {
                a_dup(u8c, q, u8bData(qbuf));
                urow.query[0] = q[0];
                urow.query[1] = q[1];
            }
            {
                a_dup(u8c, h, hex);
                urow.fragment[0] = h[0];
                urow.fragment[1] = h[1];
            }
        }
        ron60 verb = SNIFFAtVerbGet();
        ok64 ar = SNIFFAtAppendAt(ctx.ts, verb, &urow);
        if (ar != OK) {
            GET_BUFS_FREE();
            return ar;
        }

        //  Advance the keeper-side local-branch tip with verb `post`.
        //  Failure here is non-fatal: the worktree update below
        //  proceeds, and the next `be get` re-resolves via the
        //  peer-prefixed row.  HOMEBranchDir was a bare `call()` whose
        //  early return on error (e.g. PATHNOROOM on an over-long
        //  store path) skipped GET_BUFS_FREE(), leaking 4×256 MB mmaps
        //  + the subs heap (MEM-026).  Route it through the SAME
        //  non-fatal `try()` path as the adjacent REFSAppendVerb: on
        //  failure skip the keeper-tip advance entirely (keepdir is
        //  unusable) and fall through with `__ = OK`.
        a_path(keepdir);
        try(HOMEBranchDir, keepdir, NULL);
        then {
            a_pad(u8, key_buf, 128);
            u8bFeed1(key_buf, '?');
            if ($ok(source) && !u8csEmpty(source) && *source[0] == '?' &&
                $len(source) != 41) {
                a_dup(u8c, ref_q, source);
                u8csUsed1(ref_q);
                a_cstr(refs_pfx, "refs/");
                if ($len(ref_q) > 5 &&
                    memcmp(ref_q[0], refs_pfx[0], 5) == 0)
                    u8csUsed(ref_q, 5);
                u8bFeed(key_buf, ref_q);
            }
            a_dup(u8c, key_s, u8bData(key_buf));
            try(REFSAppendVerb, $path(keepdir), REFSVerbPost(), key_s, hex);
        }
        __ = OK;  //  explicit non-fatal — see comment above
    }
    //  --- end commit point --------------------------------------

    //  Open the per-module row table (BRO-002): the commit-range banner
    //  rows, the WALK's per-file rows, and the drain rows below all
    //  append to this one accumulator.  Mode-keyed: tty streams each row
    //  live; --tlv/relay buffers and flushes ONE hunk at Close.  Subs
    //  recurse as separate `be` processes (the relay re-emits their one
    //  hunk path-prefixed) so this module pass owns no sub rows — closing
    //  before returning is the per-module flush boundary.  The header uri
    //  stays EMPTY: a checkout's address is the wt itself, and the relay
    //  rebases an empty uri to the mount subpath (the module address) —
    //  carrying the target's detached `?<sha>` ref here instead would
    //  give the relay a path-less uri it can only echo, not anchor.
    rows table = {};
    u8cs empty_uri = {};
    call(ROWSOpen, &table, empty_uri, 0, 0, ROWS_MODE_KEYED);

    //  Banner: list commits crossing into / out of the wt as ULOG
    //  rows before the per-file rows from WALKTreeLazy/drains land
    //  underneath.  Skipped on first checkout (no baseline commit)
    //  and on same-commit refreshes (sha1Eq inside).
    if (has_base_commit)
        get_emit_commit_list(&base_commit_sha, &tgt_commit_sha, ctx.ts);

    o = WALKTreeLazy(tree_sha.data, get_visit, &ctx);
    if (o == OK && ctx.error != OK) o = ctx.error;
    if (o != OK) {
        (void)ROWSClose(&table);
        GET_BUFS_FREE();
        return o;
    }

    //  Drain the weave-merge list now that checkout has skipped these
    //  paths, leaving wt edits live for graf to read.  Open graf
    //  read-only — already-open is fine (idempotent).
    if (has_base_commit && u8bDataLen(merges) > 0) {
        ok64 go = GRAFOpen(NO);
        b8 own_open = (go == OK);
        if (go == OK || go == GRAFOPEN || go == GRAFOPENRO) {
            a_dup(u8c, mlist, u8bData(merges));
            (void)get_drain_merges(reporoot, mlist,
                                   &base_commit_sha, &tgt_commit_sha,
                                   ctx.ts);
            if (own_open) GRAFClose();
        }
    }

    //  Prune the baseline-only paths the classifier flagged as clean.
    {
        a_dup(u8c, ulist, u8bData(unlinks));
        (void)get_drain_unlinks(reporoot, ulist, ctx.ts);
    }
    u8bUnMap(noop); u8bUnMap(unlinks); u8bUnMap(merges); u8bUnMap(blob);

    //  Submodule orchestration moved to beagle's BEGet wrapper
    //  (SUBS.plan.md §GET).  Sniff still classifies `160000` entries
    //  during the WALK (`get_visit` returns WALKSKIP and skips
    //  materialisation), but the actual fetch/mount/recurse for each
    //  declared sub runs in `be` after sniff GET releases the keeper
    //  write lock — that avoids the mid-WALK keeper-state hazard
    //  that broke `be get` tip-switches with unintroduced subs (see
    //  get/12).  `--nosub` propagation now lives at the `be` layer.
    u8bFree(subs);
    b8 sub_fail = NO;

    //  `--force` tracked-orphan sweep (GET-016 Part 2): drop every
    //  wt-only path that sniff itself wrote (mtime ∈ stamp-set) but the
    //  target tree no longer carries — the drifted-orphan the empty
    //  baseline↔target delta would otherwise never reset.  Skipped when
    //  `--prune` is also set: prune's broader sweep below removes these
    //  paths too (plus untracked clutter), so running both is redundant.
    //  Runs on the freshly-advanced baseline, same as the prune sweep.
    if (SNIFF.force && !SNIFF.prune) {
        prune_ctx fctx = {.dropped = 0};
        u8csMv(fctx.reporoot, reporoot);
        (void)SNIFFClassify(force_orphan_cb, &fctx);
        if (fctx.dropped > 0)
            fprintf(stderr,
                    "sniff: reset %u orphaned tracked file(s)\n",
                    fctx.dropped);
    }

    //  `--prune` sweep (https://replicated.wiki/html/wiki/Verbs.html §"--force and --prune"): drop every
    //  wt-only path that isn't in the target tree.  Runs after the
    //  `get` row is appended so `SNIFFClassify` resolves the
    //  baseline to the freshly-checked-out tree — `CLASS_WT_ONLY`
    //  then means "on disk, not in target".  Gitignored paths and
    //  the `.be/` metadata dir are filtered upstream by
    //  `SNIFFWtULog → SNIFFSkipMeta`; we reload `.gitignore` first
    //  so the target tree's rules apply (a freshly added pattern
    //  must spare its matches even on this same `be get`).
    if (SNIFF.prune) {
        IGNOFree(&SNIFF.ignores);
        zerop(&SNIFF.ignores);
        a_dup(u8c, wt_for_ig, u8bDataC(HOME.wt));
        (void)IGNOLoad(&SNIFF.ignores, wt_for_ig);

        prune_ctx pctx = {.dropped = 0};
        u8csMv(pctx.reporoot, reporoot);
        (void)SNIFFClassify(prune_cb, &pctx);
        if (pctx.dropped > 0)
            fprintf(stderr,
                    "sniff: pruned %u file(s)\n", pctx.dropped);
    }

    //  Per-module flush: emit the one accumulated table hunk (--tlv) or
    //  finalise the live stream (tty) BEFORE returning to the `be`
    //  wrapper that recurses into subs.
    (void)ROWSClose(&table);

    fprintf(stderr, "sniff: checkout done\n");
    if (sub_fail) fail(SNIFFFAIL);
    done;
}
// --- Mode: Get summary (bare `be get`) ---
//
//  Bare `be get` (no URI) prints a worktree-anchored snapshot of
//  what's reachable: every local branch tip from keeper REFS, with
//  the current branch starred; then every remote-tracking ref so
//  the user can see which `//host?ref` rows are on file.

static ok64 sniff_get_branch_cb(keep_tipcp t, void *ctx) {
    u8cs *cp = (u8cs *)ctx;
    u8cs cur = {(*cp)[0], (*cp)[1]};
    char marker = u8csEq(t->path, cur) ? '*' : ' ';
    fprintf(stdout, "%c ?%.*s\t%.*s\n",
            marker,
            (int)$len(t->path), (char *)t->path[0],
            (int)$len(t->sha),  (char *)t->sha[0]);
    return OK;
}

static ok64 sniff_get_remote_cb(keep_remotecp r, void *ctx) {
    (void)ctx;
    fprintf(stdout, "  %.*s\t%.*s\n",
            (int)$len(r->key), (char *)r->key[0],
            (int)$len(r->sha), (char *)r->sha[0]);
    return OK;
}

ok64 SNIFFGetSummary(u8cs reporoot) {
    sane($ok(reporoot));
    (void)reporoot;
    keeper *k = &KEEP;

    //  Current branch from sniff baseline (empty == trunk).
    ron60 bts = 0, bverb = 0;
    uri bu = {};
    u8cs cur = {};
    if (SNIFFAtBaseline(&bts, &bverb, &bu) == OK) {
        u8csMv(cur, bu.query);
    }

    fprintf(stdout, "branches:\n");
    ok64 to = KEEPEachTip(sniff_get_branch_cb, &cur);
    if (to != OK && to != REFSNONE) {
        fprintf(stderr, "sniff: get: branches: %s\n", ok64str(to));
        fail(to);
    }

    fprintf(stdout, "remotes:\n");
    ok64 ro = KEEPEachRemote(sniff_get_remote_cb, NULL);
    if (ro != OK && ro != REFSNONE) {
        fprintf(stderr, "sniff: get: remotes: %s\n", ok64str(ro));
        fail(ro);
    }
    done;
}

// --- Mode: Checkout ---

ok64 SNIFFCheckout(u8cs reporoot, u8cs hex) {
    sane($ok(hex));
    a_pad(u8, src, 256);
    u8bFeed1(src, '?');
    u8bFeed(src, hex);
    a_dup(u8c, source, u8bData(src));
    return GETCheckout(reporoot, hex, source);
}

// Checkout from a parsed URI: resolve ?ref via keeper REFS, then checkout.
//
//  Resolution strategy (keeper WIREFetch no longer records per-origin
//  aliases — only `?heads/X → ?<sha>` and `?tags/X → ?<sha>`):
//
//    URI has ?query:
//      1. Try REFSResolve on the full URI (picks up any legacy
//         origin-qualified entries, plus the resolver's own
//         authority+query variant matcher).
//      2. Fallback: REFSResolve on a local-only `?<query>` (with a
//         leading `refs/` stripped) — lets `?refs/tags/v1` / `?v1` /
//         `?heads/main` all hit keeper's local refs file.
//      3. Last resort: treat query as a raw 40-hex SHA (covers the
//         `?<40hex>` URI that BEGetWorktree rewrites to).
//
//    URI has no ?query (fresh clone / re-clone):
//      1. If sniff at.log has a branch, resolve `?heads/<branch>`.
//      2. Else scan local REFS for a `?heads/*` entry, preferring
//         master/main/trunk; its sha is the checkout target and its
//         key becomes the at.log `source` so the branch is recorded.
static ok64 sniff_get_by_refkey(u8cs reporoot, u8csc keepdir,
                                 u8csc refkey) {
    a_pad(u8, arena, 1024);
    uri resolved = {};
    ok64 o = REFSResolve(&resolved, arena, keepdir, refkey);
    if (o != OK || $empty(resolved.query)) return KEEPNONE;
    return GETCheckout(reporoot, resolved.query, refkey);
}

// --- Path+query GET helpers -----------------------------------------
//
//  Both helpers are no-staging (no `.be/wtlog` row) overwrites of wt
//  files from another branch's tip.  Single-file form fetches one
//  blob via `KEEPGetByURI`; subtree form drains the target tree's
//  leaves via `KEEPTreeULog` and writes every leaf under the
//  requested prefix.

static ok64 sniff_get_blob_to_wt_switch(uri *u) {
    sane(u);
    //  URI-001 Stage 4b: the query IS the branch scope — the located
    //  `?<branch>/<sha>` form is retired, and SNIFFMaybeSwitch* probe
    //  the on-disk shard, no-opping when the target isn't a branch
    //  (bare sha, tag).  No string heuristic needed.
    u8cs target = {};
    u8csMv(target, u->query);
    call(SNIFFMaybeSwitchGraf,   target);
    call(SNIFFMaybeSwitchKeeper, target);
    done;
}

static ok64 sniff_get_blob_to_wt(u8cs reporoot, uri *u) {
    //  Same cross-branch consideration as the subtree overlay below:
    //  `be get file.c?feat` reads a blob from feat's tree, so feat's
    //  packs must be loaded.
    (void)sniff_get_blob_to_wt_switch(u);
    sane(u);
    keeper *k = &KEEP;
    a_carve(u8, blob, 64UL << 20);
    ok64 go = KEEPGetByURI(u, blob);
    if (go != OK) {
        fprintf(stderr,
            "sniff: get: cannot resolve %.*s?%.*s\n",
            (int)$len(u->path),  (char const *)u->path[0],
            (int)$len(u->query), (char const *)u->query[0]);
        return go;
    }
    a_path(fp);
    a_dup(u8c, rr_s, reporoot);
    call(PATHu8bFeed, fp, rr_s);
    a_dup(u8c, path_s, u->path);
    //  u->path is multi-segment (e.g. "sub/file.txt"); use Add
    //  (segment-by-segment) — PATHu8bPush would reject the embedded
    //  '/' and fail PATHBAD.  Mirrors sniff_get_subtree_to_wt.
    call(PATHu8bAdd, fp, path_s);
    //  mkdir -p the parent dir: a scoped get of a file whose directory
    //  has been removed must recreate it (GET-013), else FILECreate
    //  fails FILENONE on the absent parent.
    a_path(parent);
    a_dup(u8c, fp_s, u8bDataC(fp));
    call(PATHu8bFeed, parent, fp_s);
    call(PATHu8bPop, parent);
    (void)FILEMakeDirP($path(parent));
    int fd = -1;
    ok64 co = FILECreate(&fd, $path(fp));
    if (co != OK) {
        fprintf(stderr, "sniff: get: cannot open %.*s for write: %s\n",
                (int)u8bDataLen(fp), (char const *)u8bDataHead(fp),
                ok64str(co));
        return co;
    }
    a_dup(u8c, body, u8bData(blob));
    ok64 wo = FILEFeedAll(fd, body);
    FILEClose(&fd);
    if (wo != OK) {
        fprintf(stderr,
            "sniff: get: write %.*s failed: %s\n",
            (int)$len(u->path), (char const *)u->path[0],
            ok64str(wo));
        return wo;
    }
    fprintf(stderr,
        "sniff: get: %.*s overwritten from ?%.*s (no staging)\n",
        (int)$len(u->path),  (char const *)u->path[0],
        (int)$len(u->query), (char const *)u->query[0]);
    done;
}

//  Resolve `?ref` (URI's data) to the commit's tree sha-1.  Mirror of
//  graf/LOG.c's commit→tree extractor; lives here to avoid a graf
//  dependency in sniff for one helper.
static ok64 sniff_get_subtree_resolve_tree(uri *u, sha1 *tree_out) {
    sane(u && tree_out);
    keeper *k = &KEEP;
    a_path(keepdir);
    call(HOMEBranchDir, keepdir, NULL);

    a_pad(u8, arena, 1024);
    uri resolved = {};
    a_dup(u8c, in_uri, u->data);
    ok64 ro = REFSResolve(&resolved, arena, $path(keepdir), in_uri);
    if (ro != OK || u8csLen(resolved.query) != 40) return SNIFFNONE;

    sha1 commit_sha = {};
    {
        u8s sb = {commit_sha.data, commit_sha.data + 20};
        a_dup(u8c, hx, resolved.query);
        if (HEXu8sDrainSome(sb, hx) != OK) return SNIFFFAIL;
    }
    a_carve(u8, cbuf, 1UL << 20);
    u8 ot = 0;
    ok64 go = KEEPGetExact(&commit_sha, cbuf, &ot);
    if (go != OK || ot != DOG_OBJ_COMMIT) {
        return go == OK ? SNIFFFAIL : go;
    }
    a_dup(u8c, scan, u8bDataC(cbuf));
    u8cs field = {}, value = {};
    b8 got = NO;
    while (GITu8sDrainCommit(scan, field, value) == OK) {
        if (u8csEmpty(field)) break;
        a_cstr(ft, "tree");
        if ($eq(field, ft) && u8csLen(value) >= 40) {
            u8s sb = {tree_out->data, tree_out->data + 20};
            a_dup(u8c, hx2, value);
            if (HEXu8sDrainSome(sb, hx2) == OK) got = YES;
            break;
        }
    }
    return got ? OK : SNIFFFAIL;
}

static ok64 sniff_get_subtree_to_wt(u8cs reporoot, uri *u) {
    sane(u);
    keeper *k = &KEEP;

    //  Cross-branch overlay: when the URI's query names a different
    //  branch (`be get src/?feat`), re-target the read context to it.
    //  URI-001 Stage 4b: the query IS the branch scope (located
    //  `?<branch>/<sha>` retired); SNIFFMaybeSwitch* probe the on-disk
    //  shard and no-op on a non-branch.  Read-only context switch, no
    //  DATA shuffling for writes.
    {
        u8cs target = {};
        u8csMv(target, u->query);
        call(SNIFFMaybeSwitchGraf,   target);
        call(SNIFFMaybeSwitchKeeper, target);
    }

    sha1 tree_sha = {};
    ok64 tr = sniff_get_subtree_resolve_tree(u, &tree_sha);
    if (tr != OK) {
        fprintf(stderr,
            "sniff: get: cannot resolve %.*s?%.*s\n",
            (int)$len(u->path),  (char const *)u->path[0],
            (int)$len(u->query), (char const *)u->query[0]);
        return tr;
    }

    //  Drain the target tree's full leaf set.  KEEPTreeULog's verb
    //  stem is arbitrary — we only read uri.path / uri.fragment from
    //  each row.
    a_cstr(stem_s, "leaf");
    a_dup(u8c, stem_dup, stem_s);
    ron60 v_leaf = 0;
    call(RONutf8sDrain, &v_leaf, stem_dup);
    a_carve(u8, ulog, 4UL << 20);
    call(KEEPTreeULog, tree_sha.data, 0, v_leaf, ulog);

    //  Filter rows by the requested subtree prefix.  Path slice
    //  carries the trailing `/`; KEEPTreeULog emits paths with no
    //  trailing slash, so `<prefix>` matches `<prefix>/<...>` after
    //  comparing the prefix bytes including the final `/`.
    u8cs prefix = {u->path[0], u->path[1]};
    a_carve(u8, blob, 64UL << 20);
    u32 n_written = 0;
    a_dup(u8c, scan, u8bData(ulog));
    while (!u8csEmpty(scan)) {
        ulogrec rec = {};
        ok64 dr = ULOGu8sDrain(scan, &rec);
        if (dr == NODATA) break;
        if (dr != OK) continue;
        u8cs rp = {rec.uri.path[0], rec.uri.path[1]};
        if ($len(rp) <= $len(prefix)) continue;
        if (!u8csHasPrefix(rp, prefix)) continue;
        if (u8csLen(rec.uri.fragment) != 40) continue;

        sha1 leaf_sha = {};
        {
            u8s sb = {leaf_sha.data, leaf_sha.data + 20};
            a_dup(u8c, hx, rec.uri.fragment);
            if (HEXu8sDrainSome(sb, hx) != OK) continue;
        }
        u8bReset(blob);
        u8 ot = 0;
        if (KEEPGetExact(&leaf_sha, blob, &ot) != OK) continue;

        a_path(fp);
        a_dup(u8c, rr_s, reporoot);
        call(PATHu8bFeed, fp, rr_s);
        //  rp is multi-segment (e.g. "src/x.c"); use Add (segment-
        //  by-segment) — PATHu8bPush would reject the embedded '/'.
        a_dup(u8c, path_s, rp);
        call(PATHu8bAdd, fp, path_s);
        //  mkdir -p the parent dir.
        a_path(parent);
        a_dup(u8c, fp_s, u8bDataC(fp));
        call(PATHu8bFeed, parent, fp_s);
        call(PATHu8bPop, parent);
        (void)FILEMakeDirP($path(parent));

        int fd = -1;
        ok64 co = FILECreate(&fd, $path(fp));
        if (co != OK) {
            fprintf(stderr,
                "sniff: get: cannot open %.*s for write: %s\n",
                (int)u8bDataLen(fp), (char const *)u8bDataHead(fp),
                ok64str(co));
            continue;
        }
        a_dup(u8c, body, u8bData(blob));
        (void)FILEFeedAll(fd, body);
        FILEClose(&fd);
        n_written++;
    }

    fprintf(stderr,
        "sniff: get: %u file(s) under %.*s overwritten from ?%.*s "
        "(no staging, no prune)\n",
        n_written,
        (int)$len(u->path),  (char const *)u->path[0],
        (int)$len(u->query), (char const *)u->query[0]);
    done;
}

ok64 SNIFFGetURI(u8cs reporoot, uri *u) {
    sane(u);
    keeper *k = &KEEP;
    a_path(keepdir);
    call(HOMEBranchDir, keepdir, NULL);

    //  Absolute-form query (`?/<project>/<branch>...`) carries a
    //  project prefix that's local-side state (already consumed by
    //  home_open_inner / be_ensure_project_repo).  Strip it so the
    //  branch-side resolution paths below see just the branch
    //  portion; otherwise REFSResolve misses the row and the
    //  raw-hex fallback treats `/U` as a sha prefix.  Per https://replicated.wiki/html/wiki/Verbs.html
    //  §"Ref resolution".  Two shapes hit this gate:
    //    - Canonic resolved form (https://replicated.wiki/html/wiki/URI.html §"URI structure"):
    //          /<project>/<branch>/<pin>
    //      → DOGCanonQueryParse splits into project/branch/pin.
    //        Pin is dropped here — REFSResolve will re-derive the
    //        tip from the branch's REFS row.  (Race-protection via
    //        the canonic pin lands in a later phase.)
    //    - User-typed absolute `?/<project>[/<branch>]`
    //      → DOGQueryStripProject peels the first segment.
    //  Both update u->query AND u->data — REFSResolve re-parses
    //  u->data internally so a query-only strip wouldn't reach it.
    if (!u8csEmpty(u->query) && u->query[0][0] == '/') {
        u8cs c_proj = {}, c_branch = {}, c_pin = {};
        if (DOGCanonQueryParse(u->query, c_proj, c_branch, c_pin))
            u8csMv(u->query, c_branch);
        else
            DOGQueryStripProject(u->query);
        //  A pure project selector (`?/proj`) leaves an EMPTY branch —
        //  the `?/proj` slot picks the store shard (consumed locally /
        //  conveyed to the peer), it is NOT a local ref to resolve.
        //  Drop the now-empty query so downstream takes the fresh-clone
        //  tip-checkout path (the fetched shard's `get` row carries the
        //  remote refname → local tip) instead of a trunk ref lookup
        //  that would miss on a remote-`master` vs local-trunk mismatch
        //  and fail with SNIFFFAIL.  A `?/proj/branch` form keeps its
        //  non-empty branch and resolves normally.
        if (u8csEmpty(u->query)) {
            u->query[0] = NULL;
            u->query[1] = NULL;
        }
        //  Recompose u->data from the stripped components.  Stack
        //  buffer outlives the function since we never store the
        //  slice past this scope's REFSResolve uses (resolved.*
        //  slices index into `arena`).
        static u8 _recompose_buf[1024];
        u8s into = {_recompose_buf, _recompose_buf + sizeof(_recompose_buf)};
        u8s save = {into[0], into[1]};
        if (URIutf8Feed(into, u) == OK) {
            u->data[0] = save[0];
            u->data[1] = into[0];
        }
    }

    //  Remote URI: under DOG.md §10a `be get` is the orchestrator —
    //  it already ran `keeper get URI` synchronously before forking
    //  the parallel spot/graf/sniff children.  Sniff is a worktree
    //  updater; it does not fetch from peers itself.  Standalone
    //  `sniff get ssh://...` will fail at REFSResolve below if the
    //  pack hasn't been pre-fetched — that's intentional (use `be
    //  get` for clones).

    //  Path-bearing GET, no authority (local restore / overlay).
    //  Per https://replicated.wiki/html/wiki/GET.html §GET + §"Bareword defaults" + §"Ref resolution":
    //
    //    path  + ?<branch>  → take from branch.tip (existing form)
    //    path  + ?          → take from cur project's trunk
    //                         (empty `?` = trunk per ref-resolution rule 0)
    //    path  + (no ?)     → restore from cur baseline
    //                         (bareword promoted to path because the
    //                         file is tracked in cwd; see
    //                         §"Bareword defaults")
    //
    //  All three reuse the blob/subtree fetch path; they differ only
    //  in how `u->query` is composed before sniff_get_blob_to_wt is
    //  called.  No `.be/wtlog` row is appended in any of the three
    //  (no staging — written paths land as regular user edits).
    //  Hex-shaped path is `be get <sha>` (legacy sub-mount spawn
    //  path) — skip the file-restore block and fall through to
    //  the path-only branch below (which builds `?<hex>` for
    //  GETCheckout).  Uses dog/DOG.h's DOGIsFullSha predicate (a
    //  resolved 40/64-hex object id).  A full-sha path can't be a real
    //  filename and never makes sense as a file overlay target.
    b8 path_is_full_hex = !$empty(u->path) && DOGIsFullSha(u->path);
    if (!$empty(u->path) && $empty(u->authority) && !path_is_full_hex) {
        b8 has_query_slot     = (u->query[0] != NULL);
        b8 query_slot_empty   = has_query_slot && $empty(u->query);
        //  Treat `?.` (single dot) as the cur-branch alias — same as
        //  the no-? case (restore from baseline).  Per the
        //  branch-relative rule in https://replicated.wiki/html/wiki/URI.html §"Ref resolution", `?.`
        //  is the implicit "current branch" marker.
        b8 query_is_dot       = has_query_slot && u8csLen(u->query) == 1 &&
                                u->query[0][0] == '.';
        if (query_is_dot) {
            has_query_slot = NO;
            query_slot_empty = NO;
        }

        //  Synthesize a hex query for the no-? and empty-? cases.
        //  cur_hex / trunk_hex storage lives on the stack and must
        //  outlive sniff_get_blob_to_wt, which slices through u.
        a_pad(u8, synth_qbuf, 64);
        a_pad(u8, synth_dbuf, 320);
        if (!has_query_slot) {
            //  Restore-from-baseline: pull cur's tip sha from the
            //  latest get/post/patch wtlog row.
            ron60 cts = 0, cverb = 0;
            uri cu = {};
            if (SNIFFAtCurTip(&cts, &cverb, &cu) == OK) {
                sha1hex chex = {};
                if (SNIFFAtQueryFirstSha(&cu, &chex) == OK) {
                    a_rawc(chex_s, chex);
                    u8bFeed(synth_qbuf, chex_s);
                }
            }
            if (u8bDataLen(synth_qbuf) != 40) {
                fprintf(stderr,
                    "sniff: get: cannot restore %.*s — no baseline "
                    "in `.be/wtlog`\n",
                    (int)$len(u->path), (char const *)u->path[0]);
                fail(SNIFFFAIL);
            }
        } else if (query_slot_empty) {
            //  Empty `?` → trunk (ref-resolution rule 0).  Look up
            //  trunk's tip from the project's REFS via the bare
            //  `?` key.
            a_pad(u8, ar, 1024);
            uri resolved = {};
            a_cstr(qkey, "?");
            ok64 ro = REFSResolve(&resolved, ar, $path(keepdir),
                                   qkey);
            if (ro != OK || u8csLen(resolved.query) != 40) {
                fprintf(stderr,
                    "sniff: get: cannot resolve trunk tip for %.*s?\n",
                    (int)$len(u->path), (char const *)u->path[0]);
                fail(SNIFFFAIL);
            }
            u8bFeed(synth_qbuf, resolved.query);
        }

        if (!has_query_slot || query_slot_empty) {
            //  Splice the synthesized sha into u->fragment (not
            //  u->query): KEEPResolveTree routes # to a hex lookup
            //  and ? to a REFSResolve ref-name lookup.  Rebuild
            //  u->data as `path#40hex` so the URI re-parse downstream
            //  sees the same shape.
            a_dup(u8c, q40, u8bData(synth_qbuf));
            u8bFeed(synth_dbuf, u->path);
            u8bFeed1(synth_dbuf, '#');
            u8bFeed(synth_dbuf, q40);
            a_dup(u8c, dview, u8bData(synth_dbuf));
            u8csMv(u->fragment, q40);
            u->query[0] = NULL;
            u->query[1] = NULL;
            u8csMv(u->data, dview);
        }

        b8 is_subtree = (*u8csLast(u->path) == '/');
        if (!is_subtree)
            return sniff_get_blob_to_wt(reporoot, u);
        return sniff_get_subtree_to_wt(reporoot, u);
    }

    //  Path-only URI (no authority, no query, AND path doesn't look
    //  like a tracked file) → `be get <hex>` or `be get <local-dir>`.
    //  Today this is unreachable because the dispatcher above eats
    //  every path-bearing case; left as a defensive fall-through.
    if ($empty(u->query) && $empty(u->authority) && !$empty(u->path)) {
        a_pad(u8, src, 256);
        u8bFeed1(src, '?');
        u8bFeed(src, u->path);
        a_dup(u8c, source, u8bData(src));
        return GETCheckout(reporoot, u->path, source);
    }

    //  Everything else: resolve the (canonicalised) URI against REFS
    //  and check out the resulting sha.  Treat *presence* of `?` —
    //  even with an empty query (`?` for trunk) — as an explicit ref
    //  lookup, distinct from "no query at all" (which falls through
    //  to the at-log branch resume below).
    b8 has_q = (u->query[0] != NULL);

    //  Pre-resolve relative refs (`?./X`, `?../X`, `?..`).  Storage
    //  must outlive the call (REFSResolve and GETCheckout both
    //  consume slices into u->query / u->data); _reluri rebases
    //  those into our stack-local buffer.
    a_pad(u8, abs_qbuf,    256);
    a_pad(u8, abs_databuf, 260);
    if (SNIFFAtResolveRelativeURI(u, abs_qbuf, abs_databuf, NULL) != OK)
        fail(SNIFFFAIL);

    if (has_q || !$empty(u->authority)) {
        a_pad(u8, arena1, 1024);
        uri resolved = {};
        //  REFSResolve fallback chain: leaf (k->h->cur_branch) first,
        //  then trunk, then — when the URI query names a non-sha
        //  ref — a speculative `<root>/.be/<query>/refs` dir.  The
        //  last branch covers the fresh-clone case where keeper has
        //  just fetched into `<query>` leaf but sniff has no anchor /
        //  cur_branch to derive it from.
        a_path(leaf_keepdir);
        b8 has_leaf = (!BNULL(HOME.cur_branch) &&
                       u8bDataLen(HOME.cur_branch) > 0);
        if (has_leaf) {
            a_dup(u8c, trunk_s, u8bDataC(keepdir));
            call(PATHu8bFeed, leaf_keepdir, trunk_s);
            a_dup(u8c, leaf_s, u8bDataC(HOME.cur_branch));
            call(PATHu8bAdd, leaf_keepdir, leaf_s);
        }
        ok64 o = REFSNONE;
        if (has_leaf) {
            o = REFSResolve(&resolved, arena1, $path(leaf_keepdir),
                            u->data);
        }
        if (o != OK || $empty(resolved.query)) {
            zero(resolved);
            o = REFSResolve(&resolved, arena1, $path(keepdir), u->data);
        }
        if ((o != OK || $empty(resolved.query)) && has_q &&
            !u8csEmpty(u->query)) {
            u8cs vq = {};
            u8csMv(vq, u->query);
            b8 q_is_sha = DOGIsFullSha(vq);
            if (!q_is_sha) {
                a_path(q_keepdir);
                a_dup(u8c, trunk_s, u8bDataC(keepdir));
                call(PATHu8bFeed, q_keepdir, trunk_s);
                a_dup(u8c, vq_const, vq);
                call(PATHu8bAdd, q_keepdir, vq_const);
                zero(resolved);
                o = REFSResolve(&resolved, arena1, $path(q_keepdir),
                                u->data);
            }
        }

        //  Local lookup miss → retry with shorter query prefixes.
        //  `keeper get //host?refs/heads/X` stores under a
        //  peer-prefixed key with the wire-canonical query (e.g.
        //  `?master` after wcli_wire_to_be strips `refs/heads/`,
        //  `?tags/v1` after stripping `refs/`).  User input may be
        //  fully-qualified (`?refs/heads/X`) or already-stripped.
        //  Try `refs/`-then-`refs/heads/` peels.  When the URI has
        //  no authority of its own, also probe with `.` so peer
        //  rows participate (`sniff get ?master` finds remote
        //  tracking too).
        if ((o != OK || $empty(resolved.query)) && !$empty(u->query)) {
            char const *strips[] = {"refs/heads/", "refs/", "heads/", "", NULL};
            b8 bare = $empty(u->authority);
            for (u32 si = 0; strips[si] != NULL && (o != OK ||
                                $empty(resolved.query)); si++) {
                u8cs q = {u->query[0], u->query[1]};
                size_t plen = strlen(strips[si]);
                if (plen > 0) {
                    if ($len(q) <= plen) continue;
                    if (memcmp(q[0], strips[si], plen) != 0) continue;
                    u8csUsed(q, plen);
                }
                //  Local probe with stripped query (`?<stripped>`).
                //  Build via URIutf8Feed off `u`'s parsed components so
                //  the result is consistent regardless of whether the
                //  caller (e.g. SNIFFAtResolveRelativeURI) has rewired query
                //  and data into different buffers.
                uri retry_u = *u;
                u8csMv(retry_u.query, q);
                retry_u.data[0] = retry_u.data[1] = NULL;
                a_pad(u8, retry_buf, 512);
                call(URIutf8Feed, u8bIdle(retry_buf), &retry_u);
                a_dup(u8c, retry_uri, u8bDataC(retry_buf));
                zero(resolved);
                o = REFSResolve(&resolved, arena1,
                                $path(keepdir), retry_uri);
                if (o == OK && !$empty(resolved.query)) break;
                //  Peer-relay probe (`.?<stripped>`): bare-query inputs
                //  (no authority) — feed an authority-only `.` URI so
                //  peer-prefixed tracking rows match.
                if (!bare) continue;
                uri dot_u = retry_u;
                a_cstr(dot_path, ".");
                u8csMv(dot_u.path, dot_path);
                a_pad(u8, dot_buf, 512);
                call(URIutf8Feed, u8bIdle(dot_buf), &dot_u);
                a_dup(u8c, dot_uri, u8bDataC(dot_buf));
                zero(resolved);
                o = REFSResolve(&resolved, arena1,
                                $path(keepdir), dot_uri);
            }
        }

        //  GET never creates branches on miss — absolute and relative
        //  refs alike error out when REFS has no row.  `be post ?./X`
        //  is the spec-aligned create path (per https://replicated.wiki/html/wiki/Verbs.html).
        if (o == OK && !$empty(resolved.query)) {
            a_pad(u8, src, 256);
            u8bFeed1(src, '?');
            if (has_q) {
                if (!$empty(u->query)) u8bFeed(src, u->query);
            } else if (!$empty(resolved.fragment)) {
                //  Fresh-clone path: user gave no `?ref` (e.g.
                //  `be get ssh://sniff/src/dogs`).  Carry the matched
                //  row's refname (`heads/<branch>`) into the at-log so
                //  SNIFFAtBaseline → POSTCommit → keeper REFS chain
                //  records branch-keyed local moves.
                u8bFeed(src, resolved.fragment);
            }
            a_dup(u8c, source, u8bData(src));
            return GETCheckout(reporoot, resolved.query, source);
        }
        //  Raw hex fallback when the query is a hashlet (a 6..40-hex
        //  sha prefix, e.g. `be get '?abc1234'`) that keeper resolves
        //  via prefix lookup.  SUBS-021: gate on DOGIsHashlet — a
        //  non-hashlet query (e.g. a malformed `<sha>/.<parent>`
        //  synthetic-branch ref that survived REFSResolve; the `/.par`
        //  tail makes it non-hex / over-length) must NOT be checked
        //  out verbatim.  Doing so fed the whole string into BOTH the
        //  query AND fragment of the recorded `get` row
        //  (`?<sha>/.<parent>#<sha>/.<parent>`), leaking a sha into the
        //  project slot.  The checkout itself "succeeded" only because
        //  the leading 40-hex prefix happened to match the pin via a
        //  prefix lookup.  Gating on DOGIsFullSha was too strict — it
        //  rejected legitimate sha-prefix gets (README §"sha prefix").
        //  An unresolvable ref must fall through to the clean SNIFFFAIL.
        if (!$empty(u->query) && DOGIsHashlet(u->query)) {
            a_pad(u8, qbuf, 256);
            u8bFeed1(qbuf, '?');
            u8bFeed(qbuf, u->query);
            a_dup(u8c, qkey, u8bData(qbuf));
            return GETCheckout(reporoot, u->query, qkey);
        }
        //  Present-but-empty query (`?`, trunk): explicit fail rather
        //  than falling through to the at-log resume below — the user
        //  asked for trunk and the row isn't there.
        if (has_q) fail(SNIFFFAIL);
    }

    //  Bare `be get` (no URI args at all): resume the worktree's
    //  current branch (from `--at` forwarded by `be`, parked in
    //  `KEEP.h->cur_branch` by HOMEOpen) against the local trunk row
    //  `?#<sha>`.  Empty branch == trunk → falls through to the bare
    //  `?` lookup below.
    if (u8bDataLen(HOME.cur_branch) > 0) {
        a_pad(u8, qbuf, 256);
        u8bFeed1(qbuf, '?');
        u8bFeed(qbuf, u8bDataC(HOME.cur_branch));
        a_dup(u8c, qkey, u8bData(qbuf));
        ok64 o = sniff_get_by_refkey(reporoot, $path(keepdir), qkey);
        if (o == OK) return OK;
    }

    //  Last resort: a bare `?` (trunk) lookup — catches the case of
    //  a worktree with a local trunk row but no at.log branch name yet.
    a_cstr(trunk_s, "?");
    ok64 o = sniff_get_by_refkey(reporoot, $path(keepdir), trunk_s);
    if (o == OK) return OK;

    fail(SNIFFFAIL);
}
