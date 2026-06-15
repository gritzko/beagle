//  PATCH: 3-way worktree merge via graf.
//
//  See PATCH.h for the public surface.  Implementation walks three
//  trees (fork, ours, theirs) in tandem by fetching tree bytes via
//  graf for each directory level, classifies every leaf path by
//  the {fork, ours, theirs} sha triple, and applies a worktree
//  action.  Merged bytes come from `GRAFGet <path>?<ours>&<theirs>`;
//  pass-through bytes come from `GRAFGet <path>?<theirs>`.  Sniff
//  never reads keeper directly here.
//
//  Per https://replicated.wiki/html/wiki/PATCH.html §PATCH and Invariant 2, PATCH absorbs the target
//  branch's full (fork_commit..tip) stack into cur's wt as a single
//  squash with `base = tree(arg.fork_commit)`.  `arg.fork_commit` is
//  the LCA of the target's parent-branch tip and the target's tip —
//  the commit on the parent branch where the target was forked.
//  Provenance for the commit graph is erased: the next POST emits a
//  single-parent commit anchored on the wt's pre-patch get/post tip,
//  not the absorbed branch.  The patch row itself records `theirs`
//  in its fragment so POST's bare-no-msg path can recover the
//  absorbed commits' messages and authors as defaults; this is
//  metadata only and never participates in the commit topology.
//
#include "PATCH.h"

#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/URI.h"
#include "dog/DOG.h"
#include "dog/DPATH.h"
#include "dog/WHIFF.h"
#include "graf/GRAF.h"
#include "graf/JOIN.h"
#include "graf/REBASE.h"
#include "dog/git/GIT.h"
#include "keeper/KEEP.h"
#include "keeper/REFS.h"
#include "keeper/RESOLVE.h"
#include "keeper/WALK.h"

#include "AT.h"
#include "SNIFF.h"

#define PATCH_TREE_BUF   (4UL << 20)   // 4 MB per tree body
#define PATCH_BLOB_BUF   (16UL << 20)  // 16 MB per blob
#define PATCH_MAX_ENTRIES 4096         // per directory

// --- Entry extracted from a git-format tree body ------------------

typedef struct {
    u8cs name;       // points into the owning tree-body buffer
    u8cs mode;       // same
    sha1 sha;        // raw 20 bytes
    b8   present;    // has this side got an entry with this name?
    b8   is_dir;     // mode starts with '4' (git tree-of-trees)
} entry;

//  Order by name bytes (git tree sort).  Required by abc/Sx.h's search
//  primitives; entries are sorted in place by `sort_entries` below.
fun b8 entryZ(entry const *a, entry const *b) {
    size_t la = $len(a->name), lb = $len(b->name);
    size_t ml = la < lb ? la : lb;
    int c = (ml == 0) ? 0 : memcmp(a->name[0], b->name[0], ml);
    if (c != 0) return c < 0;
    return la < lb;
}

#define X(M, n) M##entry##n
#include "abc/Bx.h"
#undef X

//  SUBS-002: is this side a 160000 gitlink (submodule pin)?  A gitlink is
//  not a blob — its delta is the submodule's own concern, re-got by the
//  parent's BEActSubsPatch gitlink-diff recursion (reuses GET), never by
//  this WEAVE tree merge.  A NULL / absent side is not a gitlink.
static b8 entry_is_gitlink(entry const *e) {
    if (e == NULL || !e->present) return NO;
    a_cstr(gl, "160000");
    return u8csEq(e->mode, gl) ? YES : NO;
}

static ok64 parse_tree(entry *out, u32 *nout, u32 cap, u8cs body) {
    sane(out && nout);
    u32 n = 0;
    u8cs obj = {body[0], body[1]};
    u8cs file = {}, esha = {};
    while (n < cap && GITu8sDrainTree(obj, file, esha, NULL) == OK) {
        u8cs mode_s = {}, name_s = {};
        if (GITu8sFileSplit(file, mode_s, name_s) != OK) continue;
        if ($empty(name_s) || u8csLen(esha) != 20) continue;
        entry *e = &out[n++];
        e->name[0] = name_s[0]; e->name[1] = name_s[1];
        e->mode[0] = mode_s[0]; e->mode[1] = mode_s[1];
        e->is_dir = ($len(mode_s) > 0 && *mode_s[0] == '4');
        e->present = YES;
        sha1Mv(&e->sha, (sha1cp)esha[0]);
    }
    *nout = n;
    done;
}

static int entry_name_cmp(u8cs a, u8cs b) {
    size_t la = $len(a), lb = $len(b);
    size_t ml = la < lb ? la : lb;
    int c = (ml == 0) ? 0 : memcmp(a[0], b[0], ml);
    if (c != 0) return c;
    if (la < lb) return -1;
    if (la > lb) return 1;
    return 0;
}

static void sort_entries(entry *arr, u32 n) {
    for (u32 i = 1; i < n; i++) {
        entry v = arr[i];
        u32 j = i;
        while (j > 0 && entry_name_cmp(arr[j - 1].name, v.name) > 0) {
            arr[j] = arr[j - 1];
            j--;
        }
        arr[j] = v;
    }
}

fun b8 sha_eq(sha1cp a, sha1cp b) {
    return memcmp(a->data, b->data, 20) == 0;
}

// --- graf fetch wrappers -------------------------------------------
//  URIs are assembled via abc/URI's `a_uri` macro so the `path?query`
//  shape stays canonical.  Query is one 40-hex sha for a tip fetch or
//  `<hex_a>&<hex_b>` for a 2-way merge fetch.

//  Fetch a tree body via graf.  Path is `<dir>/` (or `/` at the root),
//  query is the commit sha hex.  Returns OK with `into` populated, or
//  any GRAFFAIL / KEEPNONE variant on failure (caller treats those as
//  "dir absent at that commit").
static ok64 fetch_tree(u8b into, u8cs dir, sha1cp sha) {
    sane(into && sha);
    u8bReset(into);

    a_pad(u8, pbuf, 1024);
    if ($empty(dir)) {
        call(u8bFeed1, pbuf, '/');
    } else {
        call(u8bFeed, pbuf, dir);
        if (*u8csLast(dir) != '/') call(u8bFeed1, pbuf, '/');
    }
    a_dup(u8c, path, u8bData(pbuf));

    a_sha1hex(query, sha);

    a_uri(u, 0, 0, path, query, 0);
    return GRAFGet(into, u);
}

static ok64 fetch_blob(u8b into, u8cs path, sha1cp sha) {
    sane(into && sha && !$empty(path));
    u8bReset(into);

    a_sha1hex(query, sha);

    a_uri(u, 0, 0, path, query, 0);
    return GRAFGet(into, u);
}

//  3-way WEAVE merge for a single file given two commit shas plus
//  the reporoot whose wt may contribute prior PATCH bytes.  Thin
//  wrapper over `GRAFMergeWtFileTunable` with the foster-aware
//  edge set patch_walk uses.
static ok64 fetch_merge(u8b into, u8cs reporoot, u8cs path,
                        sha1cp ours, sha1cp thrs) {
    sane(into && ours && thrs && !$empty(path));
    return GRAFMergeWtFileTunable(path, reporoot, ours, thrs,
                                  DAG_EDGE_PARENT | DAG_EDGE_FOSTER,
                                  NULL, 0, into);
}

// --- Worktree writes -----------------------------------------------

//  Write `data` to `<reporoot>/<relpath>`.  `mode` is a git-style
//  ascii mode string: `"100644"` / `"100755"` / `"120000"` (symlink).
//  Creates parent dirs as needed.  Caller is responsible for stamping
//  the file's mtime via stamp_wrote after a successful write.
static ok64 write_blob(u8cs reporoot, u8csc relpath_in,
                       u8csc mode, u8csc data) {
    sane(!$empty(relpath_in));
    a_dup(u8c, relpath, relpath_in);

    a_path(fp);
    call(SNIFFFullpath, fp, reporoot, relpath);

    //  Parent dir may need creating if this is a freshly-added file
    //  living in a new subdir.
    {
        a_path(dp);
        u8cs dir = {};
        PATHu8sDir(dir, relpath);
        if (!$empty(dir)) {
            call(SNIFFFullpath, dp, reporoot, dir);
            FILEMakeDirP($path(dp));
        }
    }

    b8 is_link = ($len(mode) >= 1 && *mode[0] == '1' &&
                  $len(mode) >= 6 && mode[0][1] == '2');
    b8 is_exe  = ($len(mode) >= 6 && mode[0][1] == '0' &&
                  mode[0][2] == '0' && mode[0][3] == '7' &&
                  mode[0][4] == '5' && mode[0][5] == '5');

    if (is_link) {
        FILEUnLink($path(fp));
        //  The "blob" for a symlink is its target path; NUL-terminate
        //  in a scratch buffer so $path(target) is C-string-safe.
        a_pad(u8, target, PATH_MAX);
        size_t dl = $len(data);
        if (dl >= u8bIdleLen(target)) dl = u8bIdleLen(target) - 1;
        u8cs trim = {data[0], data[0] + dl};
        u8bFeed(target, trim);
        u8bFeed1(target, 0);
        if (FILESymLink($path(target), $path(fp)) != OK)
            fail(PATCHFAIL);
    } else {
        int fd = -1;
        call(FILECreate, &fd, $path(fp));
        //  Capture the write result, release the fd unconditionally,
        //  then propagate — a bare call() here would leak the open fd
        //  on a write failure (MEM-041, the MEM-026 close-before-return
        //  idiom).
        ok64 wo = FILEFeedAll(fd, data);
        FILEClose(&fd);
        if (wo != OK) return wo;
        if (is_exe) FILEChmod($path(fp), 0755);
    }

    done;
}

//  Remove `<reporoot>/<relpath>` from disk.  Treats a missing file as
//  success — the net state is the same as if we'd unlinked it.
static ok64 delete_blob(u8cs reporoot, u8csc relpath_in) {
    sane(!$empty(relpath_in));
    a_dup(u8c, relpath, relpath_in);

    a_path(fp);
    call(SNIFFFullpath, fp, reporoot, relpath);
    ok64 o = FILEUnLink($path(fp));
    if (o != OK && o != FILENONE) return o;
    done;
}

// --- Merge stats ---------------------------------------------------

typedef struct {
    u32   noop;
    u32   take_theirs;
    u32   merged;
    u32   merged_conflict;   // merged bytes contained <<<<<<< markers
    u32   added;
    u32   deleted;
    u32   mod_del_kept;      // one side deleted, the other modified —
                             // content side wins, warning emitted, NOT a
                             // hard conflict (PATCH still returns OK)
    u32   failed;
    //  PATCH-001: count of `put <sub>#<pin>` rows the walk staged for
    //  forward-moved gitlinks.  Each is a ULOG append at `ts`, so when
    //  >0 the final patch provenance row must move to a strictly-greater
    //  ts (ULOG refuses ts <= tail).  Folded into `take_theirs` for the
    //  absorbed/report counters.
    u32   gitlink_put;
    //  The patch row's ts, picked up-front in PATCHApply and threaded
    //  through the walk.  Every file write_blob lays down gets stamped
    //  with this ts right after the write, so the ULOG row's ts and
    //  the on-disk mtimes stay in lock-step (stamp-set invariant).
    ron60 ts;
    //  Cherry-pick base override.  For `#<sha>` the resolved fork is
    //  parent(thr) — NOT reachable from ours — so the leaf 3-way
    //  merge must use the fork blob (l->sha) as base, not auto-LCA
    //  via GRAFGet (which would re-derive a base older than parent
    //  and silently re-apply intermediate-commit diffs).  Set true
    //  for cherry-pick PATCHApply, false otherwise.
    b8    use_fork_base;
} patch_stats;

//  Emit a per-file status row (PATCH.mkd §"Reporting"): status is one
//  of applied / merged / mod / conf / modl.  (`mod` = ours diverged
//  from the merge base and theirs didn't touch it, so the file is kept
//  as-is — fork-relative, NOT "uncommitted-dirty".)  DIS-018: a genuine
//  WEAVE conflict reports `conf` and modify/delete divergence reports
//  `modl` — both bright red (slot 'S'), both return OK; the conflict
//  markers stay in the file so POST's POSTCFLCT scan is the
//  patch→test→post safety net at commit time.  Rendered through
//  `ULOGPrintStatusLine` so it shares the GET/POST banner's ULOG status
//  shape (`<date>\t<verb>\t<path>`) and palette colour — every verb has
//  an entry in dog/ULOG.c.  Conflict rows are additionally echoed to
//  stderr (loud).
static void emit_status(const char *status, u8cs path) {
    if ($empty(path)) return;
    ron60 verb = 0;
    { a_cstr(s, status); a_dup(u8c, d, s); (void)RONutf8sDrain(&verb, d); }
    ulogrec rep = {.ts = 0, .verb = verb};
    u8csMv(rep.uri.path, path);
    (void)ULOGPrintStatusLine(&rep);
    if (strcmp(status, "conf") == 0 ||
        strcmp(status, "failed") == 0) {
        fprintf(stderr, "patch\t%s\t%.*s\n",
                status, (int)$len(path), (char *)path[0]);
    }
}

//  Does the wt's on-disk blob for `childpath` differ from `base_sha`
//  (the file's committed `ours` blob)?  YES means the user / a prior
//  PATCH left uncommitted bytes that the tree-sha classification
//  cannot see.  Best-effort: any I/O error (missing file, map fail)
//  returns NO so the caller falls back to the tree-sha verdict and
//  behaviour is unchanged when we can't read the wt copy.
static b8 wt_blob_differs(u8cs reporoot, u8cs childpath, sha1cp base_sha) {
    if ($empty(childpath) || base_sha == NULL) return NO;
    a_path(fp);
    if (SNIFFFullpath(fp, reporoot, childpath) != OK) return NO;
    u8bp mapped = NULL;
    if (FILEMapRO(&mapped, $path(fp)) != OK || mapped == NULL) return NO;
    sha1 disk_sha = {};
    KEEPObjSha(&disk_sha, DOG_OBJ_BLOB, u8bDataC(mapped));
    FILEUnMap(mapped);
    return sha1Eq(&disk_sha, base_sha) ? NO : YES;
}

//  Check whether the wt's on-disk bytes for `childpath` differ from
//  `baseline_sha` (the file's blob sha at the merge baseline).  If
//  they do, the file has user / prior-PATCH edits — emit a `dirty`
//  status row.  Best-effort: any I/O error is silently ignored.
static void emit_dirty_if_changed(u8cs reporoot, u8cs childpath,
                                  sha1cp baseline_sha) {
    if (wt_blob_differs(reporoot, childpath, baseline_sha))
        emit_status("mod", childpath);
}

//  Stamp the just-written file with the patch row's ts.  Silent on
//  error — callers are best-effort.
static void stamp_wrote(u8cs reporoot, u8cs childpath, patch_stats *st) {
    if (!st || $empty(childpath)) return;
    a_path(fp);
    if (SNIFFFullpath(fp, reporoot, childpath) != OK) return;
    (void)SNIFFAtStampPath(fp, st->ts);
}

//  Scan `bytes` for a WEAVE-format conflict marker triple.
//  Valid shapes (both produced by `WEAVEEmitMerged`):
//
//    inline:      `<<<<theirs||||ours>>>>`
//    line-block:  `<<<<\n…\n||||\n…\n>>>>\n` (each marker on its own line,
//                                            re-aligned by
//                                            `weave_realign_conflicts`
//                                            when the cluster is ≥ 1/4
//                                            of the surrounding line)
//
//  Order is fixed: open `<<<<`, then at least one mid `||||`, then close
//  `>>>>`.  A bare `<<<<` (or `>>>>`) without the matching partners — as
//  appears in documentation prose like https://replicated.wiki/html/wiki/Verbs.html — is *not* a conflict.
//  A nested `<<<<` before the close aborts the candidate triple and the
//  scan continues past the inner open.
b8 SNIFFHasConflictMarker(u8cs bytes) {
    u8cp p = bytes[0];
    u8cp e = bytes[1];
    while (p + 12 <= e) {     // need at least `<x4 |x4 >x4`
        if (p[0] != '<' || p[1] != '<' || p[2] != '<' || p[3] != '<') {
            p++; continue;
        }
        //  Candidate open at `p`.  Look forward for a `||||` (no
        //  intervening `<<<<` or `>>>>`), then a `>>>>` (no
        //  intervening `<<<<`).  Multiple `||||` separators per
        //  cluster are allowed (n-way merges).
        u8cp q = p + 4;
        b8 saw_mid = NO;
        b8 saw_close = NO;
        b8 nested_open = NO;
        while (q + 4 <= e) {
            if (q[0] == '<' && q[1] == '<' && q[2] == '<' && q[3] == '<') {
                nested_open = YES; break;
            }
            if (q[0] == '>' && q[1] == '>' && q[2] == '>' && q[3] == '>') {
                if (saw_mid) saw_close = YES;
                break;
            }
            if (q[0] == '|' && q[1] == '|' && q[2] == '|' && q[3] == '|') {
                saw_mid = YES;
                q += 4; continue;
            }
            q++;
        }
        if (saw_close) return YES;
        //  Not a valid triple — skip past this `<<<<` and keep
        //  scanning.  On nested-open we resume from the inner one
        //  so it gets its own chance to match.
        p = nested_open ? q : p + 4;
    }
    return NO;
}

// --- Per-level walk ------------------------------------------------

//  Forward decl: the worker recurses via the entry wrapper below so
//  each recursion frame gets its own scratch buffers (the parent's
//  entries are still being walked when the child fires).
static ok64 patch_walk(u8cs reporoot, u8cs dir_path,
                       sha1cp fork, sha1cp our, sha1cp thr,
                       sha1cp fork_commit,
                       sha1cp our_commit,
                       sha1cp thr_commit,
                       patch_stats *st);

//  Worker for `patch_walk`.  All scratch buffers are pre-allocated by
//  the entry wrapper and released unconditionally on its exit path —
//  see CLAUDE.md §5 (resources at top of call chain).
static ok64 patch_walk_inner(u8cs reporoot, u8cs dir_path,
                             sha1cp fork, sha1cp our, sha1cp thr,
                             sha1cp fork_commit,
                             sha1cp our_commit,
                             sha1cp thr_commit,
                             patch_stats *st,
                             u8b lbuf, u8b obuf, u8b tbuf, u8b mbuf,
                             Bentry leb, Bentry oeb, Bentry teb) {
    sane(fork && our && thr && st &&
         fork_commit && our_commit && thr_commit);
    (void)fork;

    //  Missing-at-commit is not fatal — the dir just didn't exist
    //  on that side, we treat its entry set as empty.
    //  Use COMMIT shas (not the per-recursion TREE shas) — graf's
    //  get_resolve_chunk rejects non-commit objects with GETFAIL.
    //  The tree-sha tuple is consumed by the subtree-equality
    //  short-circuit at the entry level, not here.
    ok64 lo = fetch_tree(lbuf, dir_path, fork_commit);
    ok64 oo = fetch_tree(obuf, dir_path, our_commit);
    ok64 to = fetch_tree(tbuf, dir_path, thr_commit);
    (void)lo; (void)oo; (void)to;

    entry *le = entrybDataHead(leb);
    entry *oe = entrybDataHead(oeb);
    entry *te = entrybDataHead(teb);
    u32 ln = 0, on = 0, tn = 0;
    {
        a_dup(u8c, lb, u8bData(lbuf));
        a_dup(u8c, ob, u8bData(obuf));
        a_dup(u8c, tb, u8bData(tbuf));
        parse_tree(le, &ln, PATCH_MAX_ENTRIES, lb);
        parse_tree(oe, &on, PATCH_MAX_ENTRIES, ob);
        parse_tree(te, &tn, PATCH_MAX_ENTRIES, tb);
    }
    sort_entries(le, ln);
    sort_entries(oe, on);
    sort_entries(te, tn);

    //  Lockstep walk over three sorted arrays.  At each iteration
    //  we pick the smallest head-of-arrays name, collect the
    //  triple, and advance matching heads.
    u32 li = 0, oi = 0, ti = 0;
    ok64 ret = OK;
    while (ret == OK && (li < ln || oi < on || ti < tn)) {
        u8cs *cand[3] = {
            li < ln ? &le[li].name : NULL,
            oi < on ? &oe[oi].name : NULL,
            ti < tn ? &te[ti].name : NULL,
        };
        u8cs name = {NULL, NULL};
        for (int k = 0; k < 3; k++) {
            if (!cand[k]) continue;
            if ($empty(name) || entry_name_cmp(*cand[k], name) < 0) {
                name[0] = (*cand[k])[0];
                name[1] = (*cand[k])[1];
            }
        }
        entry const *l = NULL, *o = NULL, *t = NULL;
        if (li < ln && entry_name_cmp(le[li].name, name) == 0) l = &le[li++];
        if (oi < on && entry_name_cmp(oe[oi].name, name) == 0) o = &oe[oi++];
        if (ti < tn && entry_name_cmp(te[ti].name, name) == 0) t = &te[ti++];

        //  Compose the child's full relative path into a local buffer.
        a_path(childp);
        if (!$empty(dir_path)) {
            u8bFeed(childp, dir_path);
            if (*u8csLast(dir_path) != '/') u8bFeed1(childp, '/');
        }
        u8bFeed(childp, name);
        PATHu8bTerm(childp);
        a_dup(u8c, childpath, u8bData(childp));

        //  Sniff-meta paths (.be/wtlog, .be/*, .git*) never participate
        //  in a 3-way merge: they may sit in legacy trees but PATCH
        //  must not classify or write them.  Skip the entry on every
        //  side; the next POST drops them from the result tree.
        if (SNIFFSkipMeta(childpath)) continue;

        b8 any_dir = (l && l->is_dir) || (o && o->is_dir) ||
                     (t && t->is_dir);
        if (any_dir) {
            //  For MVP: only recurse when all present sides agree
            //  it's a dir.  Mixed blob/tree at the same name is a
            //  type conflict; deferred.
            if ((l && !l->is_dir) || (o && !o->is_dir) ||
                (t && !t->is_dir)) {
                fprintf(stderr,
                    "sniff: patch: type conflict at %.*s — skipped\n",
                    (int)$len(childpath), (char *)childpath[0]);
                st->failed++;
                emit_status("failed", childpath);
                continue;
            }
            sha1 lsub = l ? l->sha : (sha1){};
            sha1 osub = o ? o->sha : (sha1){};
            sha1 tsub = t ? t->sha : (sha1){};
            //  Subtree-level short-circuit: when all three sides
            //  agree on the subtree sha, every leaf below is XXX-
            //  noop by construction — skip the whole recursion.
            //  Requires all three sides present (a missing side has
            //  a zero sha that would never match a real tree sha).
            if (l && o && t &&
                sha_eq(&lsub, &osub) && sha_eq(&osub, &tsub)) {
                continue;
            }
            //  If either subtree is missing AND absent on LCA, the
            //  whole subtree is a pure add/delete — for MVP skeleton
            //  we still descend with empty-stand-in.  Real add/delete
            //  handling comes with the structural-delete pass.
            //  Pass the subtree shas unconditionally — absent sides
            //  have zeroed sha1 and fetch_tree returns empty, which
            //  the next level interprets as "dir absent on that side".
            ret = patch_walk(reporoot, childpath,
                             &lsub, &osub, &tsub,
                             fork_commit, our_commit, thr_commit,
                             st);
            continue;
        }

        //  SUBS-002: a 160000 gitlink leaf is not a blob.  Its pin delta is
        //  re-got by the parent's BEActSubsPatch gitlink-diff recursion
        //  (reuses GET, post-order) — feeding the gitlink to fetch_blob /
        //  write_blob (or resolving its sha as a ref) fails and aborts the
        //  whole parent patch (PATCHCFLCT, `bad ref ''` / `failed <sub>`).
        //  So we never feed it to the blob path.  But the parent gitlink
        //  ROW still has to absorb a forward-moved pin:
        //
        //  PATCH-001: a behind source whose only forward change is a
        //  gitlink bump (theirs pin advanced past ours) checks the sub out
        //  on disk via the recursion arm, yet the parent tree still pins
        //  the OLD commit — a half-applied state (`be status` clean, the
        //  next `be post` records nothing).  When theirs' pin moved forward
        //  relative to ours we stage a `put <childpath>#<theirs-pin>` row —
        //  exactly the gitlink-bump row `be put <sub>` writes (sniff/PUT.c
        //  sub-mount short-circuit) — so the next POST records the new pin,
        //  in lock-step with the sub checkout the recursion performs.  An
        //  unchanged gitlink, or one only ours moved, stays a no-op.
        if (entry_is_gitlink(l) || entry_is_gitlink(o) ||
            entry_is_gitlink(t)) {
            b8 thr_moved = (t != NULL) &&
                           (o == NULL || !sha_eq(&t->sha, &o->sha));
            if (thr_moved) {
                a_sha1hex(pin_hex, &t->sha);
                uri grow = {};
                grow.path[0]     = childpath[0]; grow.path[1]     = childpath[1];
                grow.fragment[0] = pin_hex[0];   grow.fragment[1] = pin_hex[1];
                ron60 put_verb = SNIFFAtVerbPut();
                ok64 ao = SNIFFAtAppendAt(st->ts, put_verb, &grow);
                if (ao == OK) {
                    st->take_theirs++;
                    st->gitlink_put++;
                    emit_status("applied", childpath);
                } else {
                    st->failed++;
                    emit_status("failed", childpath);
                }
            } else {
                st->noop++;
            }
            continue;
        }

        //  --- Leaf classification (MVP skeleton) ---
        //  le  oe  te    → action
        //  --  --  --      -----------------------------------
        //   X   X   X    → noop (unchanged on both sides)
        //   X   X   Y    → take theirs
        //   X   Y   X    → noop (ours changed; disk already has it)
        //   X   Y   Z    → merge (Y≠X, Z≠X)
        //   X   Y   Y    → both made same change → noop (== Y==disk)
        //  --   X   X    → noop (present on both; unchanged)
        //  --   --  X    → add theirs
        //  --   X   --   → noop
        //  --   X   Y    → merge (both added different content)
        //  X    --  X    → ours deleted; noop (skeleton defers)
        //  X    --  Y    → modify/delete conflict (defer)
        //  X    X  --    → theirs deleted (defer)
        //  X    Y  --    → modify/delete conflict (defer)

        b8 o_eq_l = l && o && sha_eq(&l->sha, &o->sha);
        b8 t_eq_l = l && t && sha_eq(&l->sha, &t->sha);
        b8 o_eq_t = o && t && sha_eq(&o->sha, &t->sha);

        //  PATCH-002: the tree-sha triple {l,o,t} only sees COMMITTED
        //  bytes; the wt may carry uncommitted edits the next POST will
        //  rewrite.  When theirs ALSO changed this path (`!t_eq_l`) and
        //  the committed `ours` blob matches base (`o_eq_l`), the verdict
        //  would otherwise be `take-theirs` / `noop` and silently discard
        //  the wt edit or the incoming change.  Probe the wt copy once
        //  (best-effort; I/O-error → NO) so the arms below can route a
        //  dirty path into the 3-way weave instead.
        b8 wt_dirty = (l && o && o_eq_l && !t_eq_l)
                          ? wt_blob_differs(reporoot, childpath, &o->sha)
                          : NO;

        if (l && o && t && o_eq_l && t_eq_l) {
            //  Unchanged on both sides — skip.  But the wt's
            //  on-disk bytes may carry user / prior-PATCH edits
            //  that diverged from the baseline; emit `dirty` in
            //  that case so they're reported and not silently
            //  ignored.
            st->noop++;
            emit_dirty_if_changed(reporoot, childpath, &l->sha);
            continue;
        }
        if (l && o && t && o_eq_l && !t_eq_l && !wt_dirty) {
            //  Only theirs changed AND the wt's on-disk bytes still
            //  match the committed `ours` blob (clean baseline).  GRAFGet
            //  needs the COMMIT sha in the URI query (graf's
            //  get_resolve_chunk rejects non-commit objects with
            //  GETFAIL).  Use `thr_commit` — at the root recursion
            //  `thr == thr_commit` so this is a no-op; at deeper
            //  recursion `thr` is the subdir's TREE sha and passing it
            //  produces an empty merge buf, zeroing the file on disk.
            (void)fetch_blob(mbuf, childpath, thr_commit);
            a_dup(u8c, bytes, u8bData(mbuf));
            ok64 wo = write_blob(reporoot, childpath,
                                 t->mode, bytes);
            if (wo == OK) {
                st->take_theirs++;
                stamp_wrote(reporoot, childpath, st);
                emit_status("applied", childpath);
            } else {
                st->failed++;
                emit_status("failed", childpath);
            }
            u8bReset(mbuf);
            continue;
        }
        //  `wt_dirty` (committed `ours` == base, theirs changed, wt has
        //  uncommitted edits) falls through to the diverged-merge arm
        //  below, which folds the wt's on-disk bytes in via the
        //  GRAFMergeWtFileTunable path — disjoint regions merge clean
        //  (`merged`), a true overlap reports `content-conflict`, never
        //  a silent skip.
        if (l && o && t && !o_eq_l && t_eq_l) {
            //  Only ours changed — disk already has the right bytes.
            //  ours diverged from baseline; report as `dirty` (theirs
            //  did not touch this path, the user/prior-PATCH bytes
            //  are preserved).
            st->noop++;
            emit_status("mod", childpath);
            continue;
        }
        if (l && o && t && o_eq_t) {
            //  Both made the same change.  Disk has ours already.
            //  But the wt may carry user edits / prior-PATCH bytes
            //  that diverged from the baseline blob — emit `dirty`
            //  in that case so the user sees their work preserved.
            st->noop++;
            emit_dirty_if_changed(reporoot, childpath, &l->sha);
            continue;
        }
        if (l && o && t && !t_eq_l && !o_eq_t && (!o_eq_l || wt_dirty)) {
            //  Both changed differently → 3-way WEAVE merge.  Or
            //  (PATCH-002) the committed `ours` matches base but the wt
            //  carries uncommitted edits (`wt_dirty`) and theirs changed
            //  the path — same weave, with the wt bytes folded in as the
            //  ours-side edit.
            //
            //  Two routes, picked by `use_fork_base`:
            //
            //    * Squash / merge (`use_fork_base = NO`): drive
            //      `GRAFMergeWtFileTunable` — each side's per-file
            //      weave is built from its commit's full ancestor
            //      closure (PARENT | FOSTER picks up absorbed-via-
            //      foster history so test 15's "ancestor-skip" case
            //      dedups cleanly), wt bytes fold onto ours's side
            //      as an implicit edit, WEAVEMerge then runs.
            //
            //    * Cherry-pick / rebase-one (`use_fork_base = YES`):
            //      drive `GRAFMerge3Bytes` over the three explicit
            //      blob shas at this leaf (fork = parent(picked),
            //      ours, theirs).  The fork blob bootstraps spine
            //      and the WEAVEDiff stamps each side's INS tokens
            //      with disjoint synthetic ids — same semantic the
            //      old explicit-fork-base path produced (apply just
            //      the picked commit's diff onto ours), but through
            //      WEAVE rather than JOIN.
            u8cs childext = {};
            {
                u8cp dot = NULL;
                $for(u8c, p, childpath) { if (*p == '.') dot = (u8cp)p; }
                if (dot != NULL && dot + 1 < childpath[1]) {
                    childext[0] = dot + 1;
                    childext[1] = childpath[1];
                }
            }
            if (st->use_fork_base && !wt_dirty) {
                //  GRAFMerge3Bytes merges the three COMMITTED blobs only
                //  — it does not fold the wt layer, so a `wt_dirty` path
                //  (committed `ours` == base) would lose the on-disk
                //  edit.  Such paths take the wt-folding branch below.
                Bu8 bbuf = {}, obuf = {}, tbuf = {};
                (void)u8bMap(bbuf, PATCH_BLOB_BUF);
                (void)u8bMap(obuf, PATCH_BLOB_BUF);
                (void)u8bMap(tbuf, PATCH_BLOB_BUF);
                u8 bt = 0, ot = 0, tt = 0;
                (void)KEEPGetExact(&l->sha, bbuf, &bt);
                (void)KEEPGetExact(&o->sha, obuf, &ot);
                (void)KEEPGetExact(&t->sha, tbuf, &tt);
                a_dup(u8c, bdata, u8bData(bbuf));
                a_dup(u8c, odata, u8bData(obuf));
                a_dup(u8c, tdata, u8bData(tbuf));
                (void)GRAFMerge3Bytes(bdata, odata, tdata,
                                      childext, mbuf);
                u8bUnMap(bbuf);
                u8bUnMap(obuf);
                u8bUnMap(tbuf);
            } else {
                (void)fork_commit;
                (void)GRAFMergeWtFileTunable(childpath, reporoot,
                                             our_commit, thr_commit,
                                             DAG_EDGE_PARENT |
                                                 DAG_EDGE_FOSTER,
                                             NULL, 0, mbuf);
            }
            a_dup(u8c, bytes, u8bData(mbuf));
            b8 conflict = SNIFFHasConflictMarker(bytes);
            //  Write result using theirs' mode when ours == fork mode,
            //  else ours' mode.  MVP: always ours' mode.
            ok64 wo = write_blob(reporoot, childpath,
                                 o->mode, bytes);
            if (wo == OK) {
                stamp_wrote(reporoot, childpath, st);
                if (conflict) {
                    fprintf(stderr,
                        "sniff: patch: CONFLICT (content) %.*s\n",
                        (int)$len(childpath), (char *)childpath[0]);
                    st->merged_conflict++;
                    emit_status("conf", childpath);
                } else {
                    st->merged++;
                    emit_status("merged", childpath);
                }
            } else {
                st->failed++;
                emit_status("failed", childpath);
            }
            u8bReset(mbuf);
            continue;
        }
        if (!l && !o && t) {
            //  Target added a new file — write it.  Same commit-sha
            //  requirement as the take-theirs arm above.
            (void)fetch_blob(mbuf, childpath, thr_commit);
            a_dup(u8c, bytes, u8bData(mbuf));
            ok64 wo = write_blob(reporoot, childpath,
                                 t->mode, bytes);
            if (wo == OK) {
                st->added++;
                stamp_wrote(reporoot, childpath, st);
                emit_status("applied", childpath);
            } else {
                st->failed++;
                emit_status("failed", childpath);
            }
            u8bReset(mbuf);
            continue;
        }
        if (!l && o && !t) {
            //  Ours added, theirs doesn't know — leave it.
            st->noop++;
            continue;
        }
        if (!l && o && t && !o_eq_t) {
            //  Both added the same path, different content → 3-way
            //  merge with no common base.  Same WEAVE pipeline as
            //  modify-modify; the absent fork side becomes an empty
            //  baseline that the closure walk naturally produces.
            (void)GRAFMergeWtFileTunable(childpath, reporoot,
                                         our_commit, thr_commit,
                                         DAG_EDGE_PARENT | DAG_EDGE_FOSTER,
                                         NULL, 0, mbuf);
            a_dup(u8c, bytes, u8bData(mbuf));
            b8 conflict = SNIFFHasConflictMarker(bytes);
            ok64 wo = write_blob(reporoot, childpath,
                                 o->mode, bytes);
            if (wo == OK) {
                stamp_wrote(reporoot, childpath, st);
                if (conflict) {
                    fprintf(stderr,
                        "sniff: patch: CONFLICT (add/add) %.*s\n",
                        (int)$len(childpath), (char *)childpath[0]);
                    st->merged_conflict++;
                    emit_status("conf", childpath);
                } else {
                    st->merged++;
                    emit_status("merged", childpath);
                }
            } else {
                st->failed++;
                emit_status("failed", childpath);
            }
            u8bReset(mbuf);
            continue;
        }
        //  Structural: one side absent at leaf, LCA had the path.
        //  Modify/delete asymmetry is preserved conservatively: the
        //  side with content wins, the deletion is dropped, and a
        //  warning is logged.  DIS-018: this divergence reports a
        //  distinct `modl` status (bright red) and returns OK — like
        //  a genuine `conf` content conflict, it no longer flips the
        //  exit code.  Rationale: silently losing user-edited bytes
        //  is far worse than carrying along a file the other side
        //  meant to delete — the user can re-delete in one keystroke,
        //  and a non-zero exit broke parent recursion on submodules.
        if (l && o && !t) {
            if (sha_eq(&l->sha, &o->sha)) {
                //  Theirs deleted; ours unchanged → delete from wt.
                ok64 d = delete_blob(reporoot, childpath);
                if (d == OK) {
                    st->deleted++;
                } else {
                    st->failed++;
                    emit_status("failed", childpath);
                }
            } else {
                //  Theirs deleted; ours modified → keep ours (content
                //  side wins).  Warn loudly so the user can decide
                //  whether to re-apply the deletion.
                fprintf(stderr,
                    "sniff: patch: modify/delete on %.*s — kept ours "
                    "(theirs deleted, ours modified); re-delete if "
                    "intentional\n",
                    (int)$len(childpath), (char *)childpath[0]);
                st->mod_del_kept++;
                emit_status("modl", childpath);
            }
            continue;
        }
        if (l && !o && t) {
            if (sha_eq(&l->sha, &t->sha)) {
                //  Ours deleted; theirs unchanged → leave deleted.
                st->noop++;
            } else {
                //  Ours deleted; theirs modified → write theirs
                //  (content side wins).  Warn loudly.  Use commit-sha
                //  (graf rejects non-commit shas — see take-theirs
                //  arm above).
                (void)fetch_blob(mbuf, childpath, thr_commit);
                a_dup(u8c, bytes, u8bData(mbuf));
                ok64 wo = write_blob(reporoot, childpath,
                                     t->mode, bytes);
                if (wo == OK) {
                    stamp_wrote(reporoot, childpath, st);
                    fprintf(stderr,
                        "sniff: patch: delete/modify on %.*s — wrote "
                        "theirs (ours deleted, theirs modified); "
                        "re-delete if intentional\n",
                        (int)$len(childpath), (char *)childpath[0]);
                    st->mod_del_kept++;
                    emit_status("modl", childpath);
                } else {
                    st->failed++;
                    emit_status("failed", childpath);
                }
                u8bReset(mbuf);
            }
            continue;
        }
        if (l && !o && !t) {
            //  Both sides removed it — nothing to do on disk.
            st->noop++;
            continue;
        }
    }

    return ret;
}

//  Entry wrapper for `patch_walk_inner`: owns the per-recursion scratch
//  buffers, hands them to the worker, releases unconditionally.
static ok64 patch_walk(u8cs reporoot, u8cs dir_path,
                       sha1cp fork, sha1cp our, sha1cp thr,
                       sha1cp fork_commit,
                       sha1cp our_commit,
                       sha1cp thr_commit,
                       patch_stats *st) {
    sane(1);
    //  Per-walk scratch, all BASS-carved (rewinds when patch_walk
    //  returns).  A single allocation reused across patch_walk_inner's
    //  recursion — same lifetime the malloc/free wrapper provided.
    a_carve(u8,    lbuf, PATCH_TREE_BUF);
    a_carve(u8,    obuf, PATCH_TREE_BUF);
    a_carve(u8,    tbuf, PATCH_TREE_BUF);
    a_carve(u8,    mbuf, PATCH_BLOB_BUF);
    a_carve(entry, leb,  PATCH_MAX_ENTRIES);
    a_carve(entry, oeb,  PATCH_MAX_ENTRIES);
    a_carve(entry, teb,  PATCH_MAX_ENTRIES);

    call(patch_walk_inner, reporoot, dir_path, fork, our, thr,
         fork_commit, our_commit, thr_commit, st,
         lbuf, obuf, tbuf, mbuf, leb, oeb, teb);
    done;
}

// --- Ref resolution -----------------------------------------------

//  Forward decl: needed by absolutise_query (which calls into the
//  baseline reader) and by resolve_parent_tip below.
static ok64 resolve_current_branch(u8cs out_branch);

//  Materialise the target's absolute branch path when `target_query`
//  uses a relative prefix (`./X`, `../X`, `..`).  On entry `qbuf` is
//  reset; on success `*out_q` is the slice the caller should use for
//  REFS lookup — either pointing into `qbuf` (when relative) or back
//  into the original `target_query` (when absolute / SHA / unparsable).
//  Output slice's lifetime matches qbuf's data.
static ok64 absolutise_query(u8cs out_q, path8b qbuf, u8cs target_query) {
    sane(out_q && qbuf);
    u8csMv(out_q, target_query);
    u8cs current = {};
    (void)resolve_current_branch(current);
    b8 was_rel = NO;
    call(DPATHBranchResolveRel, qbuf, current, target_query, &was_rel);
    if (was_rel) u8csMv(out_q, $path(qbuf));
    done;
}

//  Resolve `target_query` ("heads/main", "tags/v1", or a 40-hex
//  commit sha) to the 20-byte commit sha.  Annotated tags are
//  dereferenced.  Token classification + relative-ref absolutisation
//  + hashlet expansion + REFS-with-strip-retry all live in
//  KEEPResolveRef now (keeper/RESOLVE.c); this wrapper only carries
//  the patch-specific tag-deref policy on top.
static ok64 resolve_target(sha1 *out, u8cs reporoot, u8cs target_query_in) {
    sane(out && u8csOK(target_query_in));
    (void)reporoot;

    u8cs cur_branch = {};
    (void)resolve_current_branch(cur_branch);

    ok64 rr = KEEPResolveRef(out, target_query_in, cur_branch);
    if (rr != OK) {
        //  An empty query is the parent-tip probe (resolve_parent_tip)
        //  asking "does the target's parent branch resolve?"  For a flat
        //  git-imported top-level branch there is no trunk to fall back
        //  to, so the bare `?` lookup fails — that's expected, and the
        //  caller silently drops to a DAG-LCA fork base.  Don't print the
        //  misleading `bad ref ''` user error for it (SUBS-002).
        if (!u8csEmpty(target_query_in))
            fprintf(stderr,
                "sniff: patch: bad ref '%.*s'\n",
                (int)$len(target_query_in),
                (char const *)target_query_in[0]);
        fail(PATCHFAIL);
    }

resolved_ok:;

    //  If we resolved to an annotated tag, deref to the underlying
    //  commit via the `object` field.
    a_carve(u8, cbuf, 1UL << 16);
    u8 ct = 0;
    ok64 ko = KEEPGetExact(out, cbuf, &ct);
    if (ko == OK && ct == DOG_OBJ_TAG) {
        a_dup(u8c, body, u8bDataC(cbuf));
        u8cs field = {}, value = {};
        a_cstr(obj_kw, "object");
        while (GITu8sDrainCommit(body, field, value) == OK) {
            if (u8csEmpty(field)) break;
            if (u8csEq(field, obj_kw) && u8csLen(value) >= 40) {
                u8s sb2 = {out->data, out->data + 20};
                u8cs hx2 = {value[0], value[0] + 40};
                HEXu8sDrainSome(sb2, hx2);
                break;
            }
        }
    }
    done;
}

//  Read the current worktree's branch tip from sniff's ULOG.  The
//  wt's anchor commit (`ours`) lives in the most recent get/post
//  row — patch rows are skipped via SNIFFAtCurTip because their
//  fragment carries `theirs`, not `ours`.
static ok64 resolve_ours(sha1 *out) {
    sane(out);
    ron60 ts = 0, verb = 0;
    uri u = {};
    call(SNIFFAtCurTip, &ts, &verb, &u);
    sha1hex hex = {};
    if (SNIFFAtQueryFirstSha(&u, &hex) != OK) fail(PATCHFAIL);
    call(sha1FromSha1hex, out, &hex);
    done;
}

//  Pull the wt's current absolute branch path from the latest
//  baseline row.  The query carries `<branch>&<sha>...` per dog/QURY;
//  the first non-sha chunk is the branch path.  On
//  detached / branchless baselines, returns OK with `*out_branch`
//  empty (= trunk).
static ok64 resolve_current_branch(u8cs out_branch) {
    sane(out_branch);
    out_branch[0] = NULL;
    out_branch[1] = NULL;
    ron60 bts = 0, bverb = 0;
    uri bu = {};
    //  Slice lifetime matches `bu` (ULOG mmap, valid until ULOGClose).
    if (SNIFFAtCurTip(&bts, &bverb, &bu) != OK) done;
    DOGQueryBranchOnly(bu.query, out_branch);
    done;
}

//  Compute `parent_path` from an absolute branch path: drop the last
//  `/`-segment.  Empty input (trunk) yields empty (which the caller
//  treats as "no parent").  Output slice points into the input.
static void path_parent(u8cs out, u8cs abs_branch) {
    out[0] = abs_branch[0];
    out[1] = abs_branch[0];
    if ($empty(abs_branch)) return;
    u8cp last_slash = NULL;
    $for(u8c, p, abs_branch) {
        if (*p == '/') last_slash = p;
    }
    if (last_slash != NULL) out[1] = last_slash;
}

//  Resolve the target's parent-branch tip sha.  `target_query` is the
//  raw query string PATCHApply received (no leading `?`); we parse it
//  through dog/QURY against the wt's current branch to produce the
//  target's absolute branch path, take its parent (`dirname`), and
//  resolve_target on that.  Returns:
//    OK        — `*out` is the parent tip sha
//    PATCHURELT — target is a sha (no parent branch concept), or the
//                 target is at the trunk (no parent), or the parent
//                 branch can't be resolved.
static ok64 resolve_parent_tip(sha1 *out, u8cs reporoot,
                               u8cs target_query) {
    sane(out);
    //  Take the first chunk of the multi-ref query as the branch
    //  body.  A 40-hex chunk is a sha target — no parent branch.
    a_dup(u8c, q_in, target_query);
    path8s first = {};
    DOGRefDrain(q_in, first);
    if ($empty(first)) return PATCHURELT;
    if (DOGIsFullSha(first)) return PATCHURELT;

    //  Build the target's absolute branch path.  `.`-prefix refs
    //  resolve against cur via PATH primitives (Pop/Push) — branch
    //  semantics: popping past trunk yields trunk, not "..".
    //  Project-relative refs pass through verbatim.
    path8s current = {};
    (void)resolve_current_branch(current);
    a_path(abs_path);
    if (first[0][0] == '.') {
        if (!$empty(current))
            if (PATHu8bFeed(abs_path, current) != OK) return PATCHURELT;
        path8s rel = {first[0], first[1]};
        if ($len(rel) >= 2 && rel[0][0] == '.' && rel[0][1] == '/') {
            u8csUsed(rel, 2);
        } else if ($len(rel) >= 3 && rel[0][0] == '.' &&
                   rel[0][1] == '.' && rel[0][2] == '/') {
            if (PATHu8bPop(abs_path) != OK) return PATCHURELT;
            u8csUsed(rel, 3);
        } else if ($len(rel) == 2 && rel[0][0] == '.' && rel[0][1] == '.') {
            if (PATHu8bPop(abs_path) != OK) return PATCHURELT;
            u8csUsed(rel, 2);
        }
        if (!$empty(rel))
            if (PATHu8bPush(abs_path, rel) != OK) return PATCHURELT;
    } else {
        if (PATHu8bFeed(abs_path, first) != OK) return PATCHURELT;
    }

    //  Parent path = dirname(abs_path).  An empty abs_path means
    //  the target IS the trunk — there is no parent branch to fork
    //  from, so we cannot derive a fork commit.
    a_dup(u8c, abs_slice, u8bData(abs_path));
    if ($empty(abs_slice)) return PATCHURELT;
    path8s parent = {};
    path_parent(parent, abs_slice);

    //  Re-run resolve_target on the parent path's query.  parent
    //  is a slice into abs_path; copy it into a local buffer so the
    //  query bytes are stable for resolve_target's REFSResolve call.
    //  When `parent` is empty (target is a top-level branch like
    //  ?feat), the resolver looks up trunk's tip via the bare `?`
    //  REFS key — which is exactly the target's parent.
    a_path(par_buf);
    if (!$empty(parent)) call(PATHu8bFeed, par_buf, parent);
    return resolve_target(out, reporoot, $path(par_buf));
}

// --- Public entries -------------------------------------------------

//  Worktree scan: any file whose mtime is not in the ULOG stamp-set
//  counts as dirty.  Mirrors `git merge`'s "your local changes would
//  be overwritten" guard.

static ok64 patch_dirty_report(u8cs rel, void *ctx) {
    sane(ctx);
    enum { MAX_DIRTY_REPORT = 8 };
    u32 *n = (u32 *)ctx;
    (*n)++;
    if (*n <= MAX_DIRTY_REPORT)
        fprintf(stderr, "sniff: patch: dirty %.*s\n",
                (int)$len(rel), (char *)rel[0]);
    return OK;
}

static ok64 refuse_if_dirty(u8cs reporoot) {
    sane($ok(reporoot));
    u32 dirty = 0;
    call(SNIFFAtScanDirty, reporoot, patch_dirty_report, &dirty);
    if (dirty == 0) return OK;
    fprintf(stderr, "sniff: patch: refusing merge — %u dirty file(s). "
                    "stash or commit first.\n", dirty);
    return PATCHDIRTY;
}

//  TODO: PATCH-on-PATCH overlapping files (https://replicated.wiki/html/wiki/PATCH.html §PATCH).
//  Today the dirty scan refuses any file that a prior patch touched,
//  so two patches in a row must edit disjoint file sets.  The proper
//  composition is per-file weave-driven:
//
//    1. Seed a weave with the file's full ancestor-closure replay at
//       the wt's base get/post tip (spine).
//    2. For each prior patch row's tip, append the file's full
//       ancestor-closure replay as additional layers (one src per
//       commit, via GRAFFileWeave).
//    3. Render the weave into bytes; if those bytes differ from
//       what's on disk, fold the on-disk bytes in as the next layer
//       (WEAVE_WT_SRC) — captures hand-edits made between patches.
//    4. Append the new patch tip's ancestor-closure replay as the
//       final stack of layers.
//    5. WEAVEEmitMerged → write to disk; conflicts framed by the
//       4-char `<<<<` / `||||` / `>>>>` markers in the usual way.
//
//  Until that lands, the dirty refusal above gates overlapping
//  PATCH-on-PATCH so the user sees a clear error rather than a
//  silently corrupt 3-way against the wrong base.

//  Cherry-pick prep: theirs = `frag` (any ref token shape accepted by
//  GRAFResolveRef — full sha, short hex prefix, branch path), fork =
//  parent(theirs).  Reads theirs's commit body from keeper and parses
//  the first `parent <hex>` field.  Refuses on root commits (no
//  parent).  Caller has already verified `frag` is non-empty.
static ok64 resolve_cherry(sha1 *thr_out, sha1 *fork_out, u8cs frag) {
    sane(thr_out && fork_out);

    u8cs cur_branch = {};
    (void)resolve_current_branch(cur_branch);
    ok64 hr = KEEPResolveRef(thr_out, frag, cur_branch);
    if (hr != OK) {
        fprintf(stderr,
            "sniff: patch: cannot resolve cherry ref '%.*s'\n",
            (int)$len(frag), (char const *)frag[0]);
        return PATCHFAIL;
    }

    a_carve(u8, cbuf, 1UL << 16);

    u8 ct = 0;
    ok64 ko = KEEPGetExact(thr_out, cbuf, &ct);
    if (ko != OK) return ko;
    if (ct != DOG_OBJ_COMMIT) fail(PATCHFAIL);

    u8cs body = {u8bDataHead(cbuf), u8bIdleHead(cbuf)};
    u8cs field = {}, value = {};
    b8 found_parent = NO;
    while (GITu8sDrainCommit(body, field, value) == OK) {
        if ($empty(field)) break;
        if ($len(field) == 6 &&
            memcmp(field[0], "parent", 6) == 0 &&
            $len(value) >= 40) {
            u8s sb = {fork_out->data, fork_out->data + 20};
            u8cs hx = {value[0], value[0] + 40};
            HEXu8sDrainSome(sb, hx);
            found_parent = YES;
            break;
        }
    }

    if (!found_parent) {
        fprintf(stderr,
            "sniff: patch: cherry-pick of root commit unsupported\n");
        fail(PATCHFAIL);
    }
    done;
}

//  Read `commit_sha`'s body, extract its first `parent <hex>` field
//  into `parent_out`.  Returns OK on success, PATCHFAIL on root /
//  malformed commit, KEEP* on storage error.
static ok64 patch_first_parent(sha1 *parent_out, sha1cp commit_sha) {
    sane(parent_out && commit_sha);
    a_carve(u8, cbuf, 1UL << 16);
    u8 ct = 0;
    ok64 ko = KEEPGetExact(commit_sha, cbuf, &ct);
    if (ko != OK) return ko;
    if (ct != DOG_OBJ_COMMIT) return PATCHFAIL;

    u8cs body = {u8bDataHead(cbuf), u8bIdleHead(cbuf)};
    u8cs field = {}, value = {};
    b8 found = NO;
    while (GITu8sDrainCommit(body, field, value) == OK) {
        if ($empty(field)) break;
        if ($len(field) == 6 &&
            memcmp(field[0], "parent", 6) == 0 &&
            $len(value) >= 40) {
            u8s sb = {parent_out->data, parent_out->data + 20};
            u8cs hx = {value[0], value[0] + 40};
            HEXu8sDrainSome(sb, hx);
            found = YES;
            break;
        }
    }
    return found ? OK : PATCHFAIL;
}

//  Read `commit_sha`'s headers, append every parent + foster sha to
//  `out` (capped at `cap` shas — caller's responsibility to size it).
//  `*nout` is incremented by the number of refs appended; existing
//  contents preserved.  Used by `build_reachable_via_links` to expand
//  cur's reachability set across both DAG-edge kinds.
static ok64 patch_links_of(sha1 *out, u32 cap, u32 *nout,
                           sha1cp commit_sha) {
    sane(out && nout && commit_sha);
    Bu8 cbuf = {};
    call(u8bAllocate, cbuf, 1UL << 16);
    u8 ct = 0;
    ok64 ko = KEEPGetExact(commit_sha, cbuf, &ct);
    if (ko != OK) { u8bFree(cbuf); return ko; }
    if (ct != DOG_OBJ_COMMIT) { u8bFree(cbuf); return PATCHFAIL; }

    u8cs body = {u8bDataHead(cbuf), u8bIdleHead(cbuf)};
    u8cs field = {}, value = {};
    while (GITu8sDrainCommit(body, field, value) == OK) {
        if ($empty(field)) break;
        if (*nout >= cap) break;
        b8 is_parent = ($len(field) == 6 &&
                        memcmp(field[0], "parent", 6) == 0);
        b8 is_foster = ($len(field) == 6 &&
                        memcmp(field[0], "foster", 6) == 0);
        if ((is_parent || is_foster) && $len(value) >= 40) {
            sha1 *slot = &out[*nout];
            u8s sb = {slot->data, slot->data + 20};
            u8cs hx = {value[0], value[0] + 40};
            HEXu8sDrainSome(sb, hx);
            (*nout)++;
        }
    }
    u8bFree(cbuf);
    return OK;
}

//  Linear-scan membership in a `set[]` of `n` shas.  Used by the
//  rebase-one reachability BFS — n is bounded by RBASEONE_REACH_MAX.
static b8 reach_set_has(sha1cp set, u32 nset, sha1cp q) {
    for (u32 i = 0; i < nset; i++) {
        if (sha1Eq(&set[i], q)) return YES;
    }
    return NO;
}

//  BFS from `seed` over parent ∪ foster edges, populating `set[]`.
//  Caller-provided `set` array has capacity `cap` shas; `*nset` is
//  the live count.  `picked` headers are intentionally NOT followed
//  per https://replicated.wiki/html/wiki/PATCH.html §PATCH "Ancestor-skip walk" — they are dedup-only
//  and do not participate in reachability.
static ok64 build_reachable_via_links(sha1 *set, u32 cap, u32 *nset,
                                      sha1cp seed) {
    sane(set && nset && seed);
    *nset = 0;
    if (sha1empty(seed)) return OK;
    set[(*nset)++] = *seed;

    //  BFS: walk index `i` over already-enqueued shas, expanding via
    //  patch_links_of; the link list grows as we go (parents/fosters
    //  enter the same `set` array).  Bound by `cap` to avoid runaway.
    for (u32 i = 0; i < *nset && *nset < cap; i++) {
        sha1 cur = set[i];
        sha1 links[16] = {};
        u32 nl = 0;
        ok64 lo = patch_links_of(links, 16, &nl, &cur);
        if (lo != OK) continue;       //  best-effort: skip on read fail
        for (u32 k = 0; k < nl && *nset < cap; k++) {
            if (reach_set_has(set, *nset, &links[k])) continue;
            set[(*nset)++] = links[k];
        }
    }
    return OK;
}

//  Ancestor-skip walk for rebase-one (`?br#`): pick the OLDEST commit
//  on `br_tip`'s first-parent chain that is NOT already reachable
//  from `our` via parent ∪ foster edges.
//
//  Why foster matters: a previous `?br#` + `post` cycle attaches the
//  absorbed commit as a `foster` header on cur's tip — that edge is
//  in the commit body but NOT in the DAG-LCA-driven reach set, so a
//  parent-only walk picks the same commit again, doubling the foster
//  chain on the next post.  See test/patch/18-repeated-rebase and
//  test/patch/19-feature-stack-rebase for the regressions that drove
//  this rewrite.
//
//    feature: F1 ── F2 ── F3 = br_tip
//    iter 1: reach = {our..T0}; walk F3→F2→F1; F1's parent T0 ∈ reach
//            → pick F1.
//    iter 2: cur now has foster=F1; reach grows to include F1 → walk
//            F3→F2; F2's parent F1 ∈ reach → pick F2.  Without foster
//            following, reach would still be {our..T0} and the walk
//            would pick F1 again.
//
//  `picked` headers NOT followed (spec: dedup-only).  Patch-id dedup
//  as a broader safety net is a follow-up.
#define RBASEONE_MAX 4096
#define RBASEONE_REACH_MAX 4096

//  Worker: scratch buffer pre-allocated by the entry wrapper.
static ok64 resolve_rebase_one_inner(sha1 *out, sha1cp br_tip,
                                     sha1cp our, Bsha1 reach_b) {
    sane(out && br_tip && our);
    if (sha1Eq(br_tip, our)) {
        fprintf(stderr,
            "sniff: patch: rebase-one — branch tip is already "
            "reachable from cur (nothing to replay)\n");
        fail(PATCHFAIL);
    }

    sha1 *reach = sha1bDataHead(reach_b);
    u32 nreach = 0;
    (void)build_reachable_via_links(reach, RBASEONE_REACH_MAX,
                                    &nreach, our);

    if (reach_set_has(reach, nreach, br_tip)) {
        fprintf(stderr,
            "sniff: patch: rebase-one — branch tip is already "
            "reachable from cur (nothing to replay)\n");
        fail(PATCHFAIL);
    }

    //  Walk br_tip's first-parent chain; stop at the first
    //  already-reachable parent.  The cur sha at that step (= the
    //  commit whose parent is reachable) is the one to replay.
    sha1 cur = *br_tip;
    for (u32 i = 0; i < RBASEONE_MAX; i++) {
        sha1 par = {};
        call(patch_first_parent, &par, &cur);
        if (reach_set_has(reach, nreach, &par)) {
            *out = cur;
            done;
        }
        cur = par;
    }
    fprintf(stderr,
        "sniff: patch: rebase-one — chain longer than %u hops; "
        "giving up\n", RBASEONE_MAX);
    fail(PATCHFAIL);
}

static ok64 resolve_rebase_one(sha1 *out, sha1cp br_tip,
                               sha1cp our) {
    sane(1);
    //  Reach set: 4096 × 20 = 80 KB, too big for the stack frame —
    //  BASS-carved (rewinds on return).  The worker emits its own
    //  stderr for the "chain doesn't reach" PATCHFAIL arm; any
    //  propagated parent-walk error already carries its own msg.
    a_carve(sha1, reach_b, RBASEONE_REACH_MAX);
    call(resolve_rebase_one_inner, out, br_tip, our, reach_b);
    done;
}

//  DIS-030 scope classifier.  PATCH selects only the scope; provenance
//  + message moved to POST (DIS-031).  The bang `!` is lexed by URILexer
//  as an ordinary query sub-delim (RFC 3986), so a trailing `!` on the
//  query is the whole-branch modifier (branch names carry no `!`):
//
//    `?br`   query, no trailing `!`  → PATCH_SCOPE_NEXT  (one next commit)
//    `?br!`  query, trailing `!`     → PATCH_SCOPE_WHOLE (the whole branch)
//    `#sha`  fragment only           → PATCH_SCOPE_NAMED (one named commit)
//
//  The old `?br#msg` (merge) and `?br#` (rebase-one) fragment-bearing
//  query forms are retired; a query+fragment URI is no longer a PATCH
//  shape.
u8 PATCHShape(uricp u) {
    if (u == NULL) return PATCH_SHAPE_BAD;
    u8 p = URIPattern(u);
    b8 has_q = (p & URI_QUERY)    != 0;
    b8 has_f = (p & URI_FRAGMENT) != 0;
    if (has_q && !has_f) {
        //  URI-002: the query-bang (`?br!` = whole-branch) is extracted
        //  by the uniform debanger — the same tail-shed every parser
        //  uses — rather than an ad-hoc literal-`!` read.
        a_dup(u8c, q, u->query);
        if (DOGDebangSlice(q)) return PATCH_SCOPE_WHOLE;
        return PATCH_SCOPE_NEXT;
    }
    if (!has_q && has_f && !u8csEmpty(u->fragment)) return PATCH_SCOPE_NAMED;
    return PATCH_SHAPE_BAD;
}

//  Detached-wt detector.  Detached iff the cur-tip URI is `?<sha>`
//  (sha-only query, EMPTY fragment) — the explicit detached form per
//  AT.md "Branch tracking", now emitted by GET for `be get ?<sha>`
//  and bare `be get <sha>`.  The attached branch form `?<branch>#<sha>`
//  and the trunk-state form `?#<sha>` (empty query = trunk, sha in
//  fragment) are NOT detached: a branch carries a query-side ref to
//  record against, and trunk-state commits legitimately back to trunk.
//  Per https://replicated.wiki/html/wiki/Invariants.html Invariant 7, PATCH refuses on detached wts — the
//  merge would have no branch to record the absorbed sha against.
static b8 is_detached_wt(u8cs reporoot) {
    (void)reporoot;
    ron60 ts = 0, verb = 0;
    uri u = {};
    if (SNIFFAtCurTip(&ts, &verb, &u) != OK) return NO;

    //  Detached URI shape per AT.md "Branch tracking": query carries
    //  a 40-hex SHA spec with no REF, fragment empty (`?<sha>`).  This
    //  is what GET writes for a detached checkout (`be get ?<sha>` or
    //  bare `be get <sha>`); see GET.c row writer.  Distinct from the
    //  attached forms `?<branch>#<sha>` (branch in query) and the
    //  trunk-state `?#<sha>` (empty query, sha in fragment) — both of
    //  which carry a non-empty query-side ref OR a non-empty fragment,
    //  so neither trips this gate.
    if (!u8csEmpty(u.fragment)) return NO;
    a_dup(u8c, q, u.query);
    if (u8csLen(q) != 40) return NO;
    return HEXu8sValid(q);
}

//  Secondary-worktree (submodule mount) detector.  A mounted sub's
//  `<wt>/.be` is a FILE (the row-0 `repo` anchor pointing back to the
//  parent's shared store, see Submodules.mkd §"Inner workings"); a
//  primary wt's `.be` is a DIRECTORY (the local store).  A `.be` file
//  is ONLY ever a secondary-wt anchor (dog/test/HOME.c §K_FILE), so
//  the file/dir kind is the authoritative discriminator.
//
//  POST-004: a submodule mount is ALWAYS detached at the gitlink pin
//  by design — the detached-wt refusal (Invariant 7, aimed at an
//  explicit primary `be get ?<sha>`) must NOT fire there.  PATCH only
//  stages the absorbed sha into `.be/wtlog`; it writes no REFS tip, so
//  there is nothing to record against a branch.  The synthetic branch
//  is created at commit time by the FOLLOWING parent-driven POST
//  (Submodules.mkd §"Committing detached subs"; sniff/POST.c
//  POSTCommit + beagle/BE.cli.c bepost_synth_child_uri) — leaving the
//  wt detached through PATCH is correct and keeps the patch→test→post
//  invariant: nothing lands until POST, and POST is the commit gate.
static b8 wt_is_secondary(u8cs reporoot) {
    if (!$ok(reporoot)) return NO;
    a_path(be);
    if (PATHu8bFeed(be, reporoot) != OK) return NO;
    if (PATHu8bPush(be, DOG_BE_S) != OK) return NO;
    filestat fs = {};
    if (FILELStat(&fs, $path(be)) != OK) return NO;
    return fs.kind == FILE_KIND_REG ? YES : NO;
}

ok64 PATCHApply(u8cs reporoot, uricp u) {
    sane($ok(reporoot) && u != NULL);
    //  POST-004: a submodule mount is detached at its pin by design;
    //  PATCH stages the absorbed sha into wtlog (no REFS tip), and the
    //  following parent-driven POST creates/attaches the synthetic
    //  branch.  Only a detached PRIMARY wt (explicit `be get ?<sha>`)
    //  still refuses (Invariant 7).
    if (is_detached_wt(reporoot) && !wt_is_secondary(reporoot)) {
        fprintf(stderr,
            "sniff: patch: refusing on detached wt — re-attach to a "
            "branch first (be get ?<branch>)\n");
        fail(PATCHDET);
    }
    u8 shape = PATCHShape(u);
    if (shape == PATCH_SHAPE_BAD) {
        fprintf(stderr,
            "sniff: patch URI must be one of `?<br>` (one commit), "
            "`?<br>!` (whole branch), or `#<sha>` (one named commit)\n");
        fail(PATCHFAIL);
    }
    b8 cherry = (shape == PATCH_SCOPE_NAMED);
    b8 whole  = (shape == PATCH_SCOPE_WHOLE);
    a_dup(u8c, target_query_raw, u->query);
    a_dup(u8c, frag,             u->fragment);

    //  URI-002: the whole-branch `!` modifier is the LAST char of the
    //  query (`?feat!`).  Shed it via the uniform debanger so the
    //  downstream branch resolver sees a bare `feat`; the scope is
    //  already captured in `whole` (from PATCHShape, same debanger).
    if (whole) (void)DOGDebangSlice(target_query_raw);

    //  DIS-025 Stage 2: route the target ref through the keeper funnel,
    //  classify it once with REFSQueryKind, then peel to the shape the
    //  downstream resolver expects.  This replaces the ad-hoc
    //  DOGCanonQueryParse peel: REFSResolveURI + REFSQueryKind now own
    //  the trunk / detached / branch distinction (incl. the single-slash
    //  trunk form DOGCanonQueryParse rejects).  The funnel is idempotent
    //  on the canonical query `be` already produced and uses the one
    //  per-process home (the KEEP singleton) for project + cur context.
    //
    //  Peel per kind:
    //    DETACHED (`/<proj>//<sha>`, a bare sha / hashlet) → bare 40-hex
    //      pin, so resolve_target does a sha lookup and the squash base
    //      falls to LCA(cur, sha) — every commit in (cur..sha] is
    //      absorbed, not just theirs's last commit (the DIS-025 fix).
    //    BRANCH / TAG → the branch path; resolve_target re-derives the
    //      tip and resolve_parent_tip finds the parent-branch base.
    //    TRUNK → empty (resolve_target → trunk tip; resolve_parent_tip
    //      bails with no parent → LCA(cur, trunk tip)).
    //
    //  A ref the funnel can't resolve (a `?<br>/<hashlet>` located
    //  cherry, an empty cherry-only query, …) is left untouched for the
    //  absolutise + cherry-detector path below.
    u8 canon_pad[320];
    u8s canon_w = {canon_pad, canon_pad + sizeof canon_pad};
    if (!u8csEmpty(target_query_raw) && !BNULL(HOME.root) &&
        REFSResolveURI(canon_w, target_query_raw) == OK) {
        u8cs canon = {canon_pad, canon_w[0]};
        if (!u8csEmpty(canon) && *canon[0] == '?') u8csUsed1(canon);
        refkind k = REFSQueryKind(canon);
        //  URI-001 Stage 3 forms: scope in the query, sha in the
        //  fragment (`/proj/br#<sha>`, `/proj#<sha>`), except DETACHED
        //  which is a bare query pin (`/proj/<sha>`).  Split off the
        //  `#<sha>` to recover the scope; peel to the downstream shape.
        u8cs scope = {};
        u8csMv(scope, canon);
        {
            a_dup(u8c, s, scope);
            if (u8csFind(s, '#') == OK) scope[1] = s[0];  // drop `#<sha>`
        }
        u8cs c_proj = {}, c_branch = {}, c_pin = {};
        if (k == REFKIND_DETACHED &&
            DOGCanonQueryParse(scope, c_proj, c_branch, c_pin)) {
            u8csMv(target_query_raw, c_pin);     // bare sha → cherry/sha lookup
        } else if (k == REFKIND_BRANCH || k == REFKIND_TAG) {
            //  scope = `/proj/<branch-path>` → strip project → branch.
            a_dup(u8c, br, scope);
            DOGQueryStripProject(br);
            u8csMv(target_query_raw, br);
        } else if (k == REFKIND_TRUNK) {
            target_query_raw[0] = canon_pad;     // empty = trunk
            target_query_raw[1] = canon_pad;
        }
    }

    //  Absolutise the query slot up front (`?./fix` from cur=feature
    //  → `feature/fix`) so the SNIFFMaybeSwitch* probes below see a
    //  real shard dir name, not a relative anchor.  `tq_buf` outlives
    //  every downstream read of `target_query` in this scope.
    a_pad(u8, tq_buf, 260);
    u8cs target_query = {};
    call(absolutise_query, target_query, tq_buf, target_query_raw);

    //  URI-001 Stage 4: the located cherry-pick shape (`?<branch>/<sha>`)
    //  is RETIRED — cherry-pick is the bare `#<sha>` form.  The flat
    //  per-project object pool resolves any commit without a branch
    //  locator hint, so there is no `?<branch>/<sha>` → CHERRY promotion
    //  here any more (and no `cherry_locator` to serialise).

    //  Per https://replicated.wiki/html/wiki/PATCH.html §PATCH "Weave merge into dirty wt" — PATCH no
    //  longer refuses on dirty wt.  Dirty bytes are preserved by
    //  the weave merge and reported via `patch dirty <path>` status
    //  rows (emitted from patch_walk's noop arms when the wt's
    //  on-disk bytes differ from the baseline blob).
    sha1 our_sha = {};
    call(resolve_ours, &our_sha);

    sha1 thr_sha = {};
    sha1 fork_sha = {};

    if (cherry) {
        //  Promoted CHERRY-LOCATED case: pin may be a short hashlet
        //  (6..39 hex).  Expand to a full 40-hex sha first so
        //  `resolve_cherry` (which insists on 40 chars) can run.
        if (u8csLen(frag) != 40) {
            sha1hex hex40 = {};
            call(KEEPResolveHex, &hex40, frag);
            a_rawc(hex_slice, hex40);
            u8csMv(frag, hex_slice);
        }
        call(resolve_cherry, &thr_sha, &fork_sha, frag);
    } else {
        //  Cross-branch PATCH: ensure the target branch's packs are
        //  loaded into keeper's PAST/DATA view so `resolve_target`,
        //  graf's WEAVE history walks, and the LCA / blob fetches
        //  below all resolve their objects.  No-op for tags, peer-
        //  prefixed refs, or same-branch reads.
        (void)SNIFFMaybeSwitchGraf(target_query); (void)SNIFFMaybeSwitchKeeper(target_query);
        call(resolve_target, &thr_sha, reporoot, target_query);
        //  DIS-030 scope (the branch-sourced arm — NEXT or WHOLE):
        //    PATCH_SCOPE_WHOLE (`?br!`) — absorb the full stack; keep
        //      theirs = branch tip, base = LCA (the squash/merge path).
        //    PATCH_SCOPE_NEXT  (`?br`)  — absorb ONE commit; theirs is
        //      reset to the next not-yet-replayed commit by the
        //      next-one resolver below (same engine the old rebase-one
        //      marker drove).  No fragment is read in either case.
    }

    //  Lazy-index both branches' commit chains into graf so the LCA
    //  query below sees their ancestors.  When the user invoked
    //  `keeper get` directly (no `be get` orchestration), graf hasn't
    //  seen the freshly-fetched commits yet — without this,
    //  `GRAFLca(&our, &thr)` returns 0 and PATCH refuses with
    //  PATCHURELT.  The indexer is idempotent on already-known tips.
    {
        sha1cp tips[2] = {&our_sha, &thr_sha};
        for (u32 i = 0; i < 2; i++) {
            a_sha1hex(hex_bytes, tips[i]);
            uri tip_uri = {};
            $mv(tip_uri.fragment, hex_bytes);
            $mv(tip_uri.data,     hex_bytes);
            (void)GRAFIndexFromTips(&tip_uri);
        }
    }

    //  Branch-form base resolution.  Cherry-pick already filled
    //  `fork_sha` from theirs's parent edge, so we skip this for
    //  cherry-pick.
    //
    //  Per https://replicated.wiki/html/wiki/PATCH.html §PATCH and Invariant 2: the merge base is
    //  `tree(arg.fork_commit)`, the commit on arg's parent branch
    //  where arg was forked.  We model that as
    //  `LCA(arg_parent_tip, arg_tip)` — the most recent shared
    //  ancestor between the parent branch and the target branch.
    //  This excludes ancestor commits already in cur's history,
    //  which a plain `LCA(our, theirs)` would otherwise re-revert.
    //
    //  Fallback: when the target has no parent branch in the dogs
    //  hierarchy (e.g. peer-imported flat git branches), drop down
    //  to `LCA(our, theirs)`.  The WEAVE-based merge engine in graf
    //  no longer needs an exact base — its inrm provenance recovers
    //  the spine — so the fork_sha here only steers the per-path
    //  classification in `patch_walk` (only-ours / only-theirs /
    //  diverged) and is forgiving of an over-inclusive base.
    if (!cherry) {
        sha1 parent_tip = {};
        ok64 pr = resolve_parent_tip(&parent_tip, reporoot, target_query);
        if (pr == OK) {
            call(GRAFLca, &fork_sha, &parent_tip, &thr_sha);
        }
        if (sha1empty(&fork_sha)) {
            //  No parent-branch base, or LCA returned zero.  Fall
            //  back to direct DAG-LCA of ours and theirs.
            call(GRAFLca, &fork_sha, &our_sha, &thr_sha);
        }
    }
    if (sha1empty(&fork_sha)) {
        fprintf(stderr, "sniff: patch: no common ancestor\n");
        fail(PATCHURELT);
    }

    //  Rebase-one ancestor-skip: walk first-parent chain from br.tip
    //  back to fork_sha; pick the oldest commit not reachable from
    //  cur (its parent equals fork).  Resets thr_sha to that commit;
    //  fork_sha (== parent of theirs) stays correct for the merge
    //  base.  TODO: extend reachability to follow `foster` headers
    //  on cur's history once Phase 4 lands; today the LCA-based
    //  fork captures parent-only reachability.
    if (shape == PATCH_SCOPE_NEXT) {
        sha1 br_tip = thr_sha;
        sha1 picked = {};
        //  Reachability seed = cur (our_sha), not fork_sha: prior
        //  next-one + post cycles attach absorbed commits via
        //  `foster` headers that aren't on the DAG-LCA fork chain.
        call(resolve_rebase_one, &picked, &br_tip, &our_sha);
        thr_sha = picked;
        //  Refresh fork to parent(picked) so the patch_walk tree
        //  classification uses the immediate predecessor of theirs
        //  (matches the rebase-one semantic: replay diff(parent(thr),
        //  thr)).  Files added by parent(thr) end up in fork's tree
        //  → not classified as "ours added" any more.
        sha1 new_fork = {};
        if (patch_first_parent(&new_fork, &thr_sha) == OK) {
            fork_sha = new_fork;
        }
    }

    //  Pick the patch row ts up-front.  SNIFFAtNow guarantees
    //  monotonicity against the ULOG tail (tail_ts+1 on tie).  We thread
    //  this ts through patch_walk and stamp each file SNIFFAtStampPath-
    //  style immediately after writing, so the row's ts equals every
    //  written file's mtime — the stamp-set invariant the rest of sniff
    //  (status, POST, the watch daemon) relies on.
    ron60 ts = 0;
    struct timespec tv = {};
    SNIFFAtNow(&ts, &tv);

    //  Banner: list the commits this PATCH absorbs, one
    //  `post\t?<hashlet>#<subject>` row each, before the per-file rows
    //  the walk emits below — mirrors GET's checkout banner (https://replicated.wiki/html/wiki/Verbs.html
    //  §PATCH "Reporting").  WHOLE scope absorbs a whole stack, so list
    //  theirs's commits NOT already reachable from cur (parent ∪
    //  foster) — the ancestor-skip set.  NAMED / NEXT absorb exactly one
    //  commit (thr_sha, already resolved to the picked commit above).
    //  Best-effort via `try`: a banner hiccup never fails the patch.
    if (shape == PATCH_SCOPE_NAMED || shape == PATCH_SCOPE_NEXT) {
        try(KEEPEmitCommitLine, &thr_sha, ts);
    } else {
        try(KEEPEmitCommitsSince, &our_sha, &thr_sha, ts);
    }

    //  NAMED and NEXT need the explicit-fork-base JOIN path: each
    //  absorbs a single commit's diff into ours, so the 3-way base must
    //  be parent(thr) (= parent(picked) for next-one).  fetch_merge's
    //  auto-LCA(our, thr) goes back further than parent(thr) when thr's
    //  commit chain has work that ours absorbed via foster (not an
    //  LCA-DAG ancestor) — the resulting "both sides added X" framing
    //  then misclassifies as conflict.  WHOLE keeps the auto-LCA path:
    //  it absorbs the FULL stack between LCA and theirs's tip.
    b8 explicit_fork = cherry || (shape == PATCH_SCOPE_NEXT);
    patch_stats st = { .ts = ts, .use_fork_base = explicit_fork };
    u8cs root = {NULL, NULL};   // empty dir_path → root tree
    call(patch_walk, reporoot, root,
         &fork_sha, &our_sha, &thr_sha,
         &fork_sha, &our_sha, &thr_sha,
         &st);

    //  POST-011: skip the provenance row on a TRUE noop.  A PATCH that
    //  absorbs nothing — theirs is already reachable from cur, or its
    //  objects aren't fully readable — leaves the worktree byte-identical
    //  and so has no work for the next POST to commit.  Recording a row
    //  anyway accumulates dead `patch` lines that poison the next POST
    //  (`POSTNOMSG`: the mixed-shape msg guard can't pick a message from
    //  N empty rows) with no `be` way to clear them short of editing
    //  `.be/wtlog` by hand.  A noop = every absorption counter zero
    //  (`failed` is handled by the PATCHCFLCT return below; it is zero
    //  here).  When that holds, append no row and return OK — the patch
    //  banner already reported the (absorbed-into-noop) commit line.
    u32 absorbed = st.take_theirs + st.merged + st.merged_conflict +
                   st.added + st.deleted + st.mod_del_kept + st.failed;
    if (absorbed == 0) {
        fprintf(stderr,
                "sniff: patch: noop=%u take-theirs=%u merged=%u "
                "added=%u deleted=%u content-conflict=%u mod-del-kept=%u "
                "failed=%u (nothing absorbed — no patch row staged)\n",
                st.noop, st.take_theirs, st.merged,
                st.added, st.deleted, st.merged_conflict,
                st.mod_del_kept, st.failed);
        done;
    }

    //  Append a `patch` ULOG row.  DIS-030: the row carries the resolved
    //  40-hex `<theirs>` sha plus the SCOPE, and the slot it lives in is
    //  the named-flag POST reads for provenance:
    //
    //    PATCH_SCOPE_NEXT   (`?br`)   →  `?<sha>`    (query, no `!`)
    //    PATCH_SCOPE_WHOLE  (`?br!`)  →  `?<sha>!`   (query, trailing `!`)
    //    PATCH_SCOPE_NAMED  (`#sha`)  →  `#<sha>`    (fragment)
    //
    //  A FRAGMENT-slot sha = named → POST emits `picked`.  A QUERY-slot
    //  sha = branch-sourced → POST emits `parent` (no post `!`) or
    //  `foster` (post `!`).  The trailing `!` preserves the whole-vs-next
    //  distinction for the message-reuse heuristic (a WHOLE squash has no
    //  single reusable subject).  No user merge-msg is stored at PATCH any
    //  more — message moved to POST (DIS-031).
    //
    //  URI-001 Stage 4b: rows are BARE — no `<branch>/` locator prefix;
    //  the flat per-project object pool makes every absorbed commit body
    //  readable from cur's shard.  Per-file forensic tracking lives in
    //  stamp_wrote (the row's ts matches every touched file's mtime).
    a_pad(u8, thex, 41);
    a_rawc(tsha, thr_sha);
    HEXu8sFeedSome(thex_idle, tsha);
    if (whole) u8bFeed1(thex, '!');

    uri urow = {};
    {
        a_dup(u8c, h, u8bDataC(thex));
        if (cherry) {
            //  NAMED: bare `#<sha>` in the fragment slot.
            urow.fragment[0] = h[0];
            urow.fragment[1] = h[1];
        } else {
            //  NEXT / WHOLE: `<sha>` (+ trailing `!` for WHOLE) in the
            //  query slot.
            urow.query[0] = h[0];
            urow.query[1] = h[1];
        }
    }

    ron60 verb = SNIFFAtVerbPatch();
    //  PATCH-001: the walk may have already appended `put <sub>#<pin>`
    //  rows at `ts` (forward-moved gitlinks).  ULOG refuses a row whose
    //  ts <= the tail, so the provenance row needs a strictly-greater ts
    //  when any gitlink put landed.  SNIFFAtNow yields tail+1.
    ron60 row_ts = ts;
    if (st.gitlink_put > 0) {
        struct timespec rtv = {};
        SNIFFAtNow(&row_ts, &rtv);
    }
    (void)SNIFFAtAppendAt(row_ts, verb, &urow);

    //  The absorbed-commit list was emitted up-front via
    //  SNIFFEmitCommitRange (before patch_walk); per-file rows came
    //  from the walk.  No single-tip placeholder line here anymore.

    fprintf(stderr,
            "sniff: patch: noop=%u take-theirs=%u merged=%u "
            "added=%u deleted=%u content-conflict=%u mod-del-kept=%u "
            "failed=%u\n",
            st.noop, st.take_theirs, st.merged,
            st.added, st.deleted, st.merged_conflict,
            st.mod_del_kept, st.failed);

    //  DIS-018: a content conflict (merged_conflict) no longer flips
    //  the exit code — a non-zero exit broke parent recursion when a
    //  submodule conflicted.  Conflicts are reported as `conf` (bright
    //  red) with markers left in the file; POST's POSTCFLCT scan is the
    //  patch→test→post safety net at commit time.  Only a genuine
    //  failure (I/O / write_blob) still fails the run.
    if (st.failed > 0) {
        return PATCHCFLCT;
    }
    done;
}

ok64 PATCHApplyFile(u8cs reporoot, u8cs filepath,
                    u8cs target_query, u8cs frag) {
    b8 cherry = $empty(target_query) && !$empty(frag);
    sane($ok(reporoot) && $ok(filepath) &&
         (cherry || $ok(target_query)));
    //  POST-004: skip the detached refusal for a submodule mount
    //  (secondary wt); see PATCHApply above.
    if (is_detached_wt(reporoot) && !wt_is_secondary(reporoot)) {
        fprintf(stderr,
            "sniff: patch: refusing on detached wt — re-attach to a "
            "branch first (be get ?<branch>)\n");
        fail(PATCHDET);
    }
    //  Single-file mode: just run a merge on that one path using
    //  graf's 3-way merge — no tree walk, no classification.
    sha1 our_sha = {};
    call(resolve_ours, &our_sha);
    sha1 thr_sha = {};
    if (cherry) {
        sha1 fork_unused = {};
        call(resolve_cherry, &thr_sha, &fork_unused, frag);
    } else {
        //  Cross-branch read: ensure the target branch's packs are
        //  loaded into keeper so resolve_target / graf's history walk
        //  can see them.  Mirrors PATCHApply.
        (void)SNIFFMaybeSwitchGraf(target_query);
        (void)SNIFFMaybeSwitchKeeper(target_query);
        call(resolve_target, &thr_sha, reporoot, target_query);
    }

    //  Lazy-index both commit chains into graf so GRAFMergeWtFileTunable's
    //  history walk sees both sides' ancestors.  Without this the merge
    //  silently returns ours's bytes (no theirs-side edit visible).
    //  Mirror of PATCHApply's tip-indexing block.
    {
        sha1cp tips[2] = {&our_sha, &thr_sha};
        for (u32 i = 0; i < 2; i++) {
            a_sha1hex(hex_bytes, tips[i]);
            uri tip_uri = {};
            $mv(tip_uri.fragment, hex_bytes);
            $mv(tip_uri.data,     hex_bytes);
            (void)GRAFIndexFromTips(&tip_uri);
        }
    }

    a_carve(u8, mbuf, PATCH_BLOB_BUF);
    ok64 mo = fetch_merge(mbuf, reporoot, filepath, &our_sha, &thr_sha);
    if (mo != OK) return mo;
    a_dup(u8c, bytes, u8bData(mbuf));
    b8 conflict = SNIFFHasConflictMarker(bytes);

    //  Mode fallback: reuse whatever's on disk.  Not perfect (a
    //  newly-added file has no on-disk mode yet) — fine for MVP.
    a_cstr(default_mode, "100644");
    ok64 wo = write_blob(reporoot, filepath, default_mode, bytes);
    if (wo != OK) return wo;

    //  Stamp the file so it counts as patch-written for POST's
    //  classification (not "dirty/untracked").  ts is fresh — path-
    //  scoped PATCH doesn't write a patch row (per https://replicated.wiki/html/wiki/Verbs.html §"Path-
    //  scoped PATCH": no header recorded), so the stamp is purely a
    //  baseline-marker for POST's mtime lookup.
    {
        ron60 ts = 0;
        struct timespec tv = {};
        SNIFFAtNow(&ts, &tv);
        a_path(fp);
        if (SNIFFFullpath(fp, reporoot, filepath) == OK) {
            (void)SNIFFAtStampPath(fp, ts);
        }
    }

    if (conflict) {
        //  DIS-018: report `conf` (bright red) and return OK — markers
        //  stay in the file for POST's POSTCFLCT scan.  No non-zero exit
        //  (it broke parent recursion on a submodule conflict).
        fprintf(stderr, "sniff: patch: CONFLICT (content) %.*s\n",
                (int)$len(filepath), (char *)filepath[0]);
        emit_status("conf", filepath);
        done;
    }
    emit_status("applied", filepath);
    done;
}
