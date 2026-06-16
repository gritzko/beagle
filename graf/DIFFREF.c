//  URI-driven diff: file or whole tree, ref vs wt or ref vs ref.
//  Companion to DIFF.c (file-pair) and BLAME.c (weave).
//
#include "GRAF.h"
#include "graf/BLOB.h"
#include "graf/WEAVE.h"

#include <stdio.h>
#include <string.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/RON.h"
#include "abc/URI.h"
#include "dog/DOG.h"
#include "dog/HUNK.h"
#include "dog/git/IGNO.h"
#include "dog/ULOG.h"
#include "keeper/KEEP.h"
#include "keeper/WALK.h"

#define DIFFREF_PATH_MAX  256
//  Cap for the ref-vs-ref tree diff (`diff:?from#to`).  The wt-vs-base
//  path streams from the visitor (no cap), but ref-vs-ref needs both
//  trees in memory to pair entries by path before diffing.
#define DIFFREF_MAX_FILES 8192

typedef struct {
    u8cs path;       //  borrowed: lives in set's arena
    sha1 sha;
    b8   is_sub;     //  DIFF-001: gitlink/submodule entry (mode 160000) —
                     //  the sha is a sub COMMIT pin, not a blob in this store
} diffref_entry;

//  Order by path bytes; required by abc/Sx.h search primitives pulled in
//  with the type family below. Entries are appended, never sorted here.
fun b8 diffref_entryZ(diffref_entry const *a, diffref_entry const *b) {
    size_t la = $len(a->path), lb = $len(b->path);
    size_t ml = la < lb ? la : lb;
    int c = (ml == 0) ? 0 : memcmp(a->path[0], b->path[0], ml);
    if (c != 0) return c < 0;
    return la < lb;
}

//  ABC type family so the per-tree entry arrays can be carved from BASS
//  instead of sitting ~320 KB each on the stack (see GRAFDiffTreeRefs).
#define X(M, n) M##diffref_entry##n
#include "abc/Bx.h"
#undef X

typedef struct {
    diffref_entry *v;
    u32            n;
    u32            cap;
    u32            overflow;
    Bu8            arena;     //  path bytes; owns backing
} diffref_set;

static ok64 diffref_set_push(diffref_set *s, u8cs path, u8cp esha,
                             b8 is_sub) {
    sane(s);
    if (s->n >= s->cap) { s->overflow++; done; }
    if (u8csEmpty(path) || u8csLen(path) >= DIFFREF_PATH_MAX) {
        s->overflow++; done;
    }
    diffref_entry *e = &s->v[s->n++];
    if (PATHu8bAren(s->arena, e->path, path) != OK) {
        s->n--; s->overflow++; done;
    }
    sha1Mv(&e->sha, (sha1cp)esha);
    e->is_sub = is_sub;
    done;
}

//  Compose `<lead><ref>` into `ubuf`.  `#` for an all-hex ref of
//  hashlet length (full or short sha — KEEPResolveTree's fragment
//  branch handles both, with `WHIFFHexHashlet60` driving keeper's
//  prefix lookup); `?` for ref names (`tags/v1`, `heads/main`) which
//  go through REFS.  URI-001 §"one rule": branch-FIRST — a ref whose
//  name is all-hex but exists as a REFS name (`GRAFRefIsName`) takes
//  the `?` path, never the hashlet `#`.
static ok64 diffref_compose_ref_uri(u8bp ubuf, u8cs ref) {
    sane(ubuf);
    u8 lead = (DOGIsHashlet(ref) && GRAFRefIsName(ref) != OK) ? '#' : '?';
    call(u8bFeed1, ubuf, lead);
    call(u8bFeed,  ubuf, ref);
    done;
}

static diffref_entry *diffref_set_find(diffref_set *s, u8cs path) {
    for (u32 i = 0; i < s->n; i++) {
        u8csc entry = {s->v[i].path[0], s->v[i].path[1]};
        if (u8csEq(entry, path)) return &s->v[i];
    }
    return NULL;
}

// --- 2-layer weave diff -------------------------------------------
//
//  The single primitive every diff path uses (wt-vs-base, ref-vs-ref
//  file, ref-vs-ref tree per-file).  Builds a 2-layer weave from
//  `from_data` (older) + `to_data` (newer) — `WEAVEFromBlob` ×2 +
//  `WEAVEDiff` (LCS + NEIL + canon) — then walks the resulting `inrm`
//  stream and emits hunks with syntax tags and `I`/`D`/` ` hili.
//
//  DIFF-003 emit scope (the producer half of the file-vs-tree split):
//    `full == NO`  → WEAVEEmitDiff: changed-hunks-only (3-line context
//                    windows).  Tree/dir scope (one call per file).
//    `full == YES` → WEAVEEmitFull: every line, change-tagged (the same
//                    no-windowing walk `cat:` uses).  A file-scoped
//                    `diff:<file>` renders the WHOLE file in place so a
//                    change reads in full surrounding context.  bro owns
//                    the highlighting; the EQ-side context lines just
//                    ride the existing `TOK_SIDE_EQ` tag the renderer
//                    already paints as plain context.
//
//  Sentinel ids: `from` layer uses `WEAVE_BASE_SRC` (any value other
//  than `WEAVE_WT_SRC` works); `to` layer uses `WEAVE_WT_SRC`.  The
//  predicates are the same as for wt-vs-base — `to` is treated as
//  "the next version after `from`" regardless of whether it's the
//  worktree or another commit's blob.

#define WEAVE_BASE_SRC 1u

static b8 wt_in_from(u32 c, void *ctx) {
    (void)ctx;
    return c != WEAVE_WT_SRC;
}
static b8 wt_in_to(u32 c, void *ctx) {
    (void)ctx; (void)c;
    return YES;
}

ok64 GRAFDiff2Layer(u8cs name, u8cs ext, u8cs from_data, u8cs to_data,
                    b8 full, u8cs navver) {
    sane($ok(name));

    //  Fast skip on byte-identical content.  Cheap u8csEq before any
    //  tokenisation; the common case in tree walks.  A full-file view
    //  of an unchanged file is just `cat:` — `diff:` has nothing to
    //  show, so the skip holds in both scopes.
    if (u8csEq(from_data, to_data)) return OK;

    call(GRAFArenaInit);

    weave wA = {}, wB = {}, wnu = {};
    if (WEAVEInit(&wA)  != OK ||
        WEAVEInit(&wB)  != OK ||
        WEAVEInit(&wnu) != OK) {
        WEAVEFree(&wA); WEAVEFree(&wB); WEAVEFree(&wnu);
        GRAFArenaCleanup();
        return NOROOM;
    }
    weave *wsrc = &wA, *wdst = &wB;

    ok64 ret = WEAVEFromBlob(wsrc, from_data, ext, WEAVE_BASE_SRC);
    if (ret == OK) ret = WEAVEFromBlob(&wnu, to_data, ext, WEAVE_WT_SRC);
    if (ret == OK) ret = WEAVEDiff(wdst, wsrc, &wnu, WEAVE_WT_SRC);
    if (ret == OK) {
        wsrc = wdst;
        if (full) {
            //  `diff:` scheme so the whole-file hunk renders as a
            //  unified diff (same +/- formatter a windowed hunk uses).
            a_cstr(diff_scheme, "diff:");
            ret = WEAVEEmitFull(wsrc, name, diff_scheme, navver,
                                wt_in_from, NULL,
                                wt_in_to,   NULL,
                                GRAFHunkEmit, NULL);
        } else {
            ret = WEAVEEmitDiff(wsrc, name, navver,
                                wt_in_from, NULL,
                                wt_in_to,   NULL,
                                GRAFHunkEmit, NULL);
        }
    }

    WEAVEFree(&wA);
    WEAVEFree(&wB);
    WEAVEFree(&wnu);
    GRAFArenaCleanup();
    return ret;
}

// --- wt-vs-base file: thin wrapper around GRAFDiff2Layer -----------

ok64 GRAFDiffWtFile(u8cs filepath, u64 base_h40, u8cs reporoot, b8 full) {
    sane($ok(filepath) && $ok(reporoot));
    keeper *k = &KEEP;

    a_carve(u8, base_buf, 16UL << 20);
    ok64 bo = GRAFBlobAtCommit(base_buf, base_h40, filepath);
    u8cs from_data = {};
    if (bo == OK) {
        a_dup(u8c, fd, u8bData(base_buf));
        u8csMv(from_data, fd);
    }

    a_path(wt_path, reporoot, filepath);
    u8bp wt_mapped = NULL;
    u8cs to_data = {};
    ok64 wto = FILEMapRO(&wt_mapped, $path(wt_path));
    if (wto == OK && wt_mapped) {
        a_dup(u8c, td, u8bData(wt_mapped));
        u8csMv(to_data, td);
    }

    u8cs ext = {};
    PATHu8sExt(ext, filepath);
    //  wt-vs-base: the file is dirty, there is no version range — emit
    //  an EMPTY navver so each hunk's nav URI stays `diff:<path>#L<n>`
    //  (clicking re-runs wt-vs-base, which is correct here).
    u8cs navver = {};
    ok64 ret = GRAFDiff2Layer(filepath, ext, from_data, to_data, full,
                              navver);

    if (wt_mapped) FILEUnMap(wt_mapped);
    return ret;
}

// --- Shared collect-into-set visitor (ref-vs-ref tree path) -------

typedef struct {
    diffref_set *set;
} diffref_collect_ctx;

static ok64 diffref_collect_visit(u8cs path, u8 kind, u8cp esha,
                                   u8cs blob, void0p ctx) {
    (void)blob;
    diffref_collect_ctx *c = (diffref_collect_ctx *)ctx;
    if (kind == WALK_KIND_REG || kind == WALK_KIND_EXE ||
        kind == WALK_KIND_LNK) {
        diffref_set_push(c->set, path, esha, NO);
    } else if (kind == WALK_KIND_SUB) {
        //  DIFF-001: a submodule gitlink — capture it so a pin bump is
        //  rendered as a change instead of vanishing from the diff.
        diffref_set_push(c->set, path, esha, YES);
    }
    return OK;
}

// --- Whole tree, wt vs base ---------------------------------------
//
//  Two ULOG streams — base via `KEEPTreeULog`, wt via `ULOGu8bScanWt`
//  — heap-merged by URI key with `ULOGMergeWalk`.  Per distinct path:
//    BOTH       compare base sha (in base row's `#fragment`) against a
//               freshly-computed wt sha; equal → skip, differ → run
//               the 2-layer weave diff.
//    BASE_ONLY  file deleted in wt → 2-layer weave diff with empty wt.
//    WT_ONLY    file added in wt   → 2-layer weave diff with empty base.
//
//  Sha-skip pays for every unchanged file: one mmap + one SHA1 vs a
//  full keeper blob fetch + tokenize + diff.

//  Skip predicate for the wt scanner.  `IGNOMatch` already handles
//  unconditional `.git/.be/.be/wtlog` skipping (at any depth, including
//  nested submodule `.git/`s) AND any `.gitignore` patterns the
//  caller loaded into the `igno` struct via `IGNOLoad(reporoot)`.
static b8 diffref_wt_skip(u8cs rel, void *ctx) {
    return IGNOMatch((ignocp)ctx, rel, NO);
}

typedef struct {
    keeper *k;
    u64     base_h40;
    u8cs    reporoot;
    ron60   v_base;
    ron60   v_wt;
    Bu8     sub_prefixes;   // newline-separated `<path>/`
    b8      sub_init;
} diffref_wt_ctx;

//  Submodule descendant filter — same pattern as sniff/CLASS.c.  When
//  the merge surfaces a gitlink row (mode `160000` in the base tree),
//  we remember `<path>/` and drop every subsequent step whose path
//  starts with that prefix.  Lex order guarantees the gitlink row
//  arrives before any of its descendants, so a single forward scan of
//  remembered prefixes is sufficient.

static b8 diffref_under_submodule(diffref_wt_ctx const *c, u8cs path) {
    if (!c->sub_init) return NO;
    a_dup(u8c, scan, u8bDataC(c->sub_prefixes));
    while (!u8csEmpty(scan)) {
        u8cs prefix = {};
        u8csMv(prefix, scan);
        a_dup(u8c, find, scan);
        if (u8csFind(find, '\n') != OK) break;
        prefix[1] = find[0];
        if (!u8csEmpty(prefix) && u8csHasPrefix(path, prefix)) return YES;
        u8csUsed1(find);
        u8csMv(scan, find);
    }
    return NO;
}

static ok64 diffref_remember_submodule(diffref_wt_ctx *c, u8cs path) {
    if (!c->sub_init) {
        //  Acquired once and reused across every subsequent step (the
        //  cross-step submodule-prefix filter).  It must OUTLIVE the
        //  per-step BASS rewind diffref_wt_step performs (MEM-018), so
        //  this acquire happens on the submodule-remember path, which
        //  returns BEFORE that step takes its mark — the buffer sits
        //  below the mark and survives.  It is released only when the
        //  outer GRAFDiffWtTree call() frame unwinds.
        ok64 ao = u8bAcquire(ABC_BASS, c->sub_prefixes, 1UL << 12);
        if (ao != OK) return ao;
        c->sub_init = YES;
    }
    (void)u8bFeed(c->sub_prefixes, path);
    (void)u8bFeed1(c->sub_prefixes, '/');
    (void)u8bFeed1(c->sub_prefixes, '\n');
    return OK;
}

static ok64 diffref_wt_step(ulogreccp recs, u32 n, void *ctx_) {
    diffref_wt_ctx *c = (diffref_wt_ctx *)ctx_;
    ulogreccp base = NULL, wt = NULL;
    for (u32 i = 0; i < n; i++) {
        if      (ok64stem(recs[i].verb) == c->v_base) base = &recs[i];
        else if (ok64stem(recs[i].verb) == c->v_wt)   wt   = &recs[i];
    }
    //  Wt-only rows are untracked files (no entry in the baseline
    //  tree).  They have no `from` side to diff against, so skip
    //  them — `be diff:` is wt-vs-baseline, not wt-vs-empty.
    if (!base) return OK;

    u8cs path = {};
    u8csMv(path, base->uri.path);
    if ($empty(path) || $len(path) >= DIFFREF_PATH_MAX) return OK;

    //  Drop wt-side rows that descend into a previously-recorded
    //  submodule (the embedded repo's own files have no business in
    //  this tree's diff).
    if (diffref_under_submodule(c, path)) return OK;

    //  Submodules / gitlink trees: remember the path as a prefix to
    //  filter, then drop the row itself.
    if (base != NULL && ok64Lit(base->verb, 0) == RON_s) {
        (void)diffref_remember_submodule(c, path);
        return OK;
    }

    //  MEM-018: ULOGMergeWalk fires this step via a raw fn-ptr (no
    //  call()/try() boundary), so any BASS scratch acquired below would
    //  accumulate across every changed path — GRAFDiffWtFile alone
    //  carves 16 MB per file — until `a_carve` returns NOROOM and the
    //  remaining diffs are silently dropped.  Mark BASS here and rewind
    //  before returning so each path's sha-skip + 16 MB diff scratch
    //  dies per step.  The mark sits ABOVE the cross-step `sub_prefixes`
    //  buffer (acquired on the submodule-remember path, which returns
    //  earlier), so that persistent filter survives the rewind.
    u8 *mark = u8aMark(ABC_BASS);

    //  BOTH: sha-skip.  Hash wt bytes once and compare with the base
    //  entry's `#<sha>` fragment.  Equal → no diff.
    if (base != NULL && wt != NULL) {
        a_path(wt_path, c->reporoot, path);
        u8bp wt_mapped = NULL;
        if (FILEMapRO(&wt_mapped, $path(wt_path)) == OK && wt_mapped) {
            a_dup(u8c, wd, u8bDataC(wt_mapped));
            sha1 wt_sha = {};
            KEEPObjSha(&wt_sha, DOG_OBJ_BLOB, wd);
            sha1 base_sha = {};
            a_dup(u8c, hx, base->uri.fragment);
            b8 same = (sha1FromHex(&base_sha, hx) == OK &&
                       sha1Eq(&wt_sha, &base_sha));
            FILEUnMap(wt_mapped);
            if (same) { u8aRewind(ABC_BASS, mark); return OK; }
        }
    }

    //  Real diff.  GRAFDiffWtFile handles the empty-base / empty-wt
    //  edge cases internally (deletion / addition both emit hunks).
    //  Tree scope → changed-hunks-only (`full == NO`); the whole-file
    //  view is reserved for an explicitly file-scoped `diff:<file>`.
    (void)GRAFDiffWtFile(path, c->base_h40, c->reporoot, NO);
    u8aRewind(ABC_BASS, mark);
    return OK;
}

#define DIFFREF_WT_BASE_BUF (1UL << 20)
#define DIFFREF_WT_WT_BUF   (1UL << 20)

ok64 GRAFDiffWtTree(u64 base_h40, u8cs base_hex, u8cs reporoot) {
    sane($ok(base_hex) && $ok(reporoot));
    keeper *k = &KEEP;

    //  Resolve base hex to its tree sha.
    a_pad(u8, ubuf, 256);
    call(diffref_compose_ref_uri, ubuf, base_hex);
    a_dup(u8c, udata, u8bData(ubuf));
    uri target = {};
    call(URIutf8Drain, udata, &target);
    sha1 base_tree = {};
    call(KEEPResolveTree, &target, &base_tree);

    a_cstr(s_base, "base");
    a_cstr(s_wt,   "wt");
    ron60 v_base = 0, v_wt = 0;
    {
        a_dup(u8c, sb, s_base); RONutf8sDrain(&v_base, sb);
        a_dup(u8c, sw, s_wt);   RONutf8sDrain(&v_wt,   sw);
    }

    a_carve(u8, bu, DIFFREF_WT_BASE_BUF);
    a_carve(u8, wu, DIFFREF_WT_WT_BUF);

    //  Base side: keeper tree → ULOG rows (`<ts>\t<verb>\t<path>?<mode>#<hex-sha>\n`).
    call(KEEPTreeULog, base_tree.data, 0, v_base, bu);

    //  Wt side: filesystem walk → ULOG rows (no sha; computed on demand).
    //  Load reporoot's `.gitignore` so build/Corpus/etc. drop out; the
    //  meta dirs (`.git/.be/.be/wtlog`) are filtered by IGNOMatch
    //  unconditionally even with no `.gitignore` present.
    igno ig = {};
    a_dup(u8c, ig_root, reporoot);
    (void)IGNOLoad(&ig, ig_root);
    ok64 wo = ULOGu8bScanWt(reporoot, v_wt,
                             diffref_wt_skip, &ig, wu);
    if (wo != OK) { IGNOFree(&ig); return wo; }

    //  Heap-merge by URI key, fan to the per-path step.
    a_dup(u8c, view_b, u8bData(bu));
    a_dup(u8c, view_w, u8bData(wu));
    a_pad(u8cs, ins, 2);
    u8csbFeed1(ins, view_b);
    u8csbFeed1(ins, view_w);
    a_dup(u8cs, cursors, u8csbData(ins));

    diffref_wt_ctx ctx = {.k = k, .base_h40 = base_h40,
                          .reporoot = {}, .v_base = v_base, .v_wt = v_wt};
    u8csMv(ctx.reporoot, reporoot);
    ok64 mr = ULOGMergeWalk(cursors, diffref_wt_step, &ctx);

    IGNOFree(&ig);
    return mr;
}

// --- Whole tree, ref vs ref ---------------------------------------

//  DIFF-001: render a submodule gitlink change `<path> <old>..<new>`
//  (empty side = added / removed sub).  The pins are sub COMMIT shas,
//  not blobs in this store, so the pin move IS the change shown here;
//  recursing into the sub for its content diff is a separate concern.
static void diffref_emit_gitlink(u8cs path, sha1cp old_sha, sha1cp new_sha) {
    u8cs os = {}, ns = {};
    sha1hex oh = {}, nh = {};
    if (old_sha) { sha1hexFromSha1(&oh, old_sha); sha1hexSlice(os, &oh); }
    if (new_sha) { sha1hexFromSha1(&nh, new_sha); sha1hexSlice(ns, &nh); }
    fprintf(stdout, "%.*s %.*s..%.*s\n",
            (int)$len(path), (char *)path[0],
            (int)$len(os), os[0] ? (char *)os[0] : "",
            (int)$len(ns), ns[0] ? (char *)ns[0] : "");
}

//  Inner worker — every early `call()` returns through here, so the
//  outer wrapper's cleanup runs on success and on every error path.
//  Buffers/arenas are caller-owned: the wrapper allocs/maps before
//  calling, frees/unmaps after, regardless of the inner's outcome.
static ok64 graf_diff_tree_refs_inner(keeper *k, u8cs from, u8cs to,
                                      diffref_set *from_set,
                                      diffref_set *to_set,
                                      Bu8 old_buf, Bu8 new_buf) {
    sane(k && from_set && to_set);

    //  DIFF-004: the per-file hunk nav URI carries this range in the
    //  query — `diff:<path>?<from>..<to>#L<n>` — so clicking a tree/
    //  commit diff's file hunk re-opens that file's RANGE diff at the
    //  line, not the empty-query wt-vs-base form.
    a_lign(u8, nav_g);
    (void)u8gFeed(nav_g, from);
    a_cstr(dots, "..");
    (void)u8gFeed(nav_g, dots);
    (void)u8gFeed(nav_g, to);
    a_cquire(u8, navver);

    // --- 1. Walk `from`, collect ---
    a_pad(u8, fbuf, 256);
    call(diffref_compose_ref_uri, fbuf, from);
    a_dup(u8c, fdata, u8bData(fbuf));
    uri ftarget = {};
    call(URIutf8Drain, fdata, &ftarget);
    diffref_collect_ctx fctx = {.set = from_set};
    call(KEEPLsFiles, &ftarget, diffref_collect_visit, &fctx);

    // --- 2. Walk `to`, collect ---
    a_pad(u8, tbuf, 256);
    call(diffref_compose_ref_uri, tbuf, to);
    a_dup(u8c, tdata, u8bData(tbuf));
    uri ttarget = {};
    call(URIutf8Drain, tdata, &ttarget);
    diffref_collect_ctx tctx = {.set = to_set};
    call(KEEPLsFiles, &ttarget, diffref_collect_visit, &tctx);

    if (from_set->overflow || to_set->overflow) {
        fprintf(stderr, "graf: diff-tree: files skipped (>%u limit)\n",
                (u32)DIFFREF_MAX_FILES);
    }

    // --- 3. For each to-entry, diff against matching from-entry ---
    for (u32 i = 0; i < to_set->n; i++) {
        u8cs path = {};
        u8csMv(path, to_set->v[i].path);
        diffref_entry *f = diffref_set_find(from_set, path);

        // Same sha on both sides → unchanged, skip cheaply.
        if (f && sha1Eq(&f->sha, &to_set->v[i].sha)) continue;

        //  DIFF-001: a gitlink entry — render the pin move (old→new),
        //  not a blob diff (the pins are sub commits, not blobs here).
        if (to_set->v[i].is_sub) {
            diffref_emit_gitlink(path, (f && f->is_sub) ? &f->sha : NULL,
                                 &to_set->v[i].sha);
            continue;
        }

        u8cs old_data = {}, new_data = {};
        if (f) {
            u8bReset(old_buf);
            u8 ot = 0;
            if (KEEPGetExact(&f->sha, old_buf, &ot) == OK && ot == DOG_OBJ_BLOB) {
                a_dup(u8c, old_dup, u8bData(old_buf));
                u8csMv(old_data, old_dup);
            }
        }
        u8bReset(new_buf);
        u8 nt = 0;
        if (KEEPGetExact(&to_set->v[i].sha, new_buf, &nt) == OK && nt == DOG_OBJ_BLOB) {
            a_dup(u8c, new_dup, u8bData(new_buf));
            u8csMv(new_data, new_dup);
        }

        u8cs ext = {};
        PATHu8sExt(ext, path);
        GRAFDiff2Layer(path, ext, old_data, new_data, NO, navver);
    }

    // --- 4. from-only entries (deletions): diff blob vs empty ---
    for (u32 i = 0; i < from_set->n; i++) {
        u8cs path = {};
        u8csMv(path, from_set->v[i].path);
        if (diffref_set_find(to_set, path) != NULL) continue;

        //  DIFF-001: a removed submodule — render the pin removal.
        if (from_set->v[i].is_sub) {
            diffref_emit_gitlink(path, &from_set->v[i].sha, NULL);
            continue;
        }

        u8bReset(old_buf);
        u8 ot = 0;
        if (KEEPGetExact(&from_set->v[i].sha, old_buf, &ot) != OK || ot != DOG_OBJ_BLOB)
            continue;
        a_dup(u8c, old_data, u8bData(old_buf));
        u8cs new_data = {};
        u8cs ext = {};
        PATHu8sExt(ext, path);
        GRAFDiff2Layer(path, ext, old_data, new_data, NO, navver);
    }
    done;
}

ok64 GRAFDiffTreeRefs(u8cs from, u8cs to, u8cs reporoot) {
    sane($ok(from) && $ok(to));
    keeper *k = &KEEP;
    (void)reporoot;

    //  Caller-owned storage so cleanup runs on every error path the
    //  inner returns through.  Each Bu8 stays zero until its alloc/map
    //  succeeds; the trailing free/unmap branch on each is gated on
    //  that — leaks on KEEPLsFiles ⇒ KEEPNONE etc are eliminated.
    a_carve(diffref_entry, from_entries_b, DIFFREF_MAX_FILES);
    a_carve(diffref_entry, to_entries_b,   DIFFREF_MAX_FILES);
    diffref_set from_set = {.v = diffref_entrybDataHead(from_entries_b),
                            .cap = DIFFREF_MAX_FILES};
    diffref_set to_set   = {.v = diffref_entrybDataHead(to_entries_b),
                            .cap = DIFFREF_MAX_FILES};

    a_carve(u8, from_set_arena, DIFFREF_MAX_FILES * DIFFREF_PATH_MAX);
    a_carve(u8, to_set_arena,   DIFFREF_MAX_FILES * DIFFREF_PATH_MAX);
    a_carve(u8, old_buf,        16UL << 20);
    a_carve(u8, new_buf,        16UL << 20);
    //  diffref_set::arena is a Bu8 (4-pointer array) — point each set's
    //  arena field at our carved buffer.
    memcpy(from_set.arena, from_set_arena, sizeof(Bu8));
    memcpy(to_set.arena,   to_set_arena,   sizeof(Bu8));

    return graf_diff_tree_refs_inner(k, from, to,
                                     &from_set, &to_set,
                                     old_buf, new_buf);
}
