//  POST: commit the current worktree state.
//
//  Inputs at commit time: the worktree on disk and the ULOG.  The most
//  recent `get` / `post` / `patch` row names a baseline tree URI
//  (single hash → keeper, multiple → graf — graf is not wired yet and
//  defaults to keeper-single-hash-only for now).  `put` / `delete`
//  rows since the last `post` are the explicit staging intent.
//
//  Per-file change-set at commit time:
//    * path matches a `put <path>` row (since last post)   → rewrite
//    * path matches a `delete <path>` row                  → drop
//    * no explicit row for the path, any put/delete exists → carry
//      over from baseline (or drop if missing from wt)
//    * no put/delete rows at all since last post           → implicit
//      all-dirty: mtime ∉ stamp-set → rewrite; missing → drop.
//
//  Pack layout: one keeper pack with `strict_order=NO`, fed in the
//  order commit → trees → blobs (forward refs permitted).
//
#include "POST.h"

#include <string.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/RON.h"
#include "dog/CLI.h"
#include "dog/DOG.h"
#include "dog/HUNK.h"
#include "dog/git/IGNO.h"
#include "dog/WHIFF.h"
#include "graf/GRAF.h"
#include "graf/REBASE.h"
#include "dog/git/GIT.h"
#include "keeper/REFS.h"
#include "keeper/RESOLVE.h"
#include "dog/git/SHA1.h"
#include "keeper/WALK.h"

#include "AT.h"
#include "GET.h"
#include "PATCH.h"

//  Cap on the per-commit ULOG-shaped buffer (decisions).  32 MiB is
//  enough for tens of thousands of distinct paths per commit at
//  ~100 bytes per row.  A repo that exceeds this at commit time has
//  bigger problems than the cap.
#define POST_TREE_ULOG_MAX (32UL << 20)

// --- Per-commit decision stream ---
//
//  `post_classify_step` runs the merge / decide / hash inline and
//  writes one decision ULOG row per distinct path into `ctx.decisions`.
//  Verbs in the stream:
//    keep    : carry baseline sha+mode verbatim (M/A/D printer is
//              silent on KEEP).
//    unlink  : drop from tree + unlink from disk.
//    add     : new content (freshly hashed); query optionally chains
//              `&<old_sha>` so the M/A/D printer can distinguish add
//              ("A") from rewrite ("M") and the blob feed can build
//              an OFS/REF_DELTA against the baseline blob.
//
//  Every consumer (tree-build, unlink loop, blob feed, stamp,
//  patch-parents, M/A/D print) drains `decisions`.

//  Decision verbs emitted into `decisions` (ron60-encoded utf8 tokens).
//  Precomputed via abc/ok64 so they're file-scope constants.
con ron60 POST_V_KEEP   = 0xbe9a74;
con ron60 POST_V_UNLINK = 0xe72c2dcaf;
con ron60 POST_V_ADD    = 0x25a28;

//  Per-commit context.  `keeper *` and `reporoot` are intentionally
//  absent: keeper is a process singleton (`KEEP`) and the repo root is
//  reachable as `u8bDataC(SNIFF.h->root)` (see dog/HOME.h §home).
typedef struct {
    Bu8            decisions;    // ULOG-shaped: <ts>\t<verb>\t<path>?<query>#<frag>
    ron60          stamp_ts;     // single per-commit stamp (post ts).
                                 // Also used to re-stamp content-clean
                                 // files whose mtime drifted, so they
                                 // align with the new post row and the
                                 // next bare `be` fast-paths them.
    struct timespec stamp_tv;    // same instant as stamp_ts in
                                 // timespec form — used for the
                                 // commit body's wall-clock seconds
                                 // so the commit object, the ULOG
                                 // post row, and the file mtimes
                                 // all share one instant (and inherit
                                 // SNIFFAtNow's monotonicity guard).
    b8             any_pd;       // any put/delete rows since last post
    b8             has_base;     // baseline get/post row exists
    ron60          last_post_ts;
    ok64           error;
} post_ctx;

//  Shorthand: the worktree root, sourced from the SNIFF singleton.
//  Used for wt-file operations (hashing, path joining).  For a
//  secondary worktree `h->wt` is this wt's on-disk path while
//  `h->root` points at the primary store — keep the two channels
//  distinct.  Macro rather than `fun` because `u8cs` is an array
//  typedef (can't be returned by value).
#define post_reporoot()  u8bDataC(SNIFF.h->wt)

//  Store root — where `.be/<project>/<branch>/` shards live.
//  Used for REFS lookups and the cascade walk.  Equals
//  `post_reporoot()` for a colocated worktree; differs for a
//  secondary worktree where the store lives on the primary.
#define post_storeroot() u8bDataC(SNIFF.h->root)

// --- git mode helpers ---

static void post_mode_feed(Bu8 tree, u16 mode) {
    //  Git modes are printed in octal without leading zeros.  All four
    //  values we emit are 5- or 6-digit strings.
    a_pad(u8, buf, 8);
    int n = snprintf((char *)u8bIdleHead(buf), u8bIdleLen(buf),
                     "%o", (unsigned)mode);
    if (n > 0) u8bFed(buf, (size_t)n);
    u8bFeed(tree, u8bDataC(buf));
}

// --- Forward decls ---

static ok64 post_emit_decision(post_ctx *c, ron60 verb,
                               u8cs path, u16 mode,
                               sha1cp old_sha,
                               sha1cp frag_sha);

// --- Hash a wt file (mmap or readlink) into a sha1 ---

//  Compute the git-blob sha for a wt file.  Symlinks (mode 120000)
//  go through `readlink`; everything else uses `FILEMapRO`.  Output
//  written into `*out`.  Errors propagate (file gone, mmap fail).
static ok64 post_hash_path(u8cs reporoot, u8cs path, u16 mode, sha1 *out) {
    sane($ok(reporoot) && out);
    a_path(fp);
    if (SNIFFFullpath(fp, reporoot, path) != OK) fail(SNIFFFAIL);

    if (mode == 0120000) {
        a_pad(u8, target, 1024);
        call(FILEReadLink, target, $path(fp));
        KEEPObjSha(out, DOG_OBJ_BLOB, u8bDataC(target));
        done;
    }

    //  Empty regular file: mmap refuses 0-byte mappings, so stat
    //  ahead and hash the empty content directly (gives the canonical
    //  empty-blob sha e69de29b…).  Without this, FILEMapRO returns
    //  non-OK and the caller silently drops the file from the commit.
    filestat fs = {};
    if (FILELStat(&fs, $path(fp)) == OK && fs.size == 0) {
        u8cs empty = {NULL, NULL};
        KEEPObjSha(out, DOG_OBJ_BLOB, empty);
        done;
    }

    u8bp mapped = NULL;
    call(FILEMapRO, &mapped, $path(fp));
    KEEPObjSha(out, DOG_OBJ_BLOB, u8bDataC(mapped));
    FILEUnMap(mapped);
    done;
}

// --- ULOG scans ---

//  YES iff `p` lives strictly beneath `prefix` (prefix must end '/').
fun b8 post_path_under(u8cs p, u8cs prefix) {
    if (u8csLen(prefix) == 0) return NO;
    if (u8csLen(p) <= u8csLen(prefix)) return NO;
    return u8csHasPrefix(p, prefix);
}

//  Walk a sorted ULOG row buffer and, for every row whose URI path
//  is strictly under `prefix`, emit a fresh ULOG row to `out` with
//  the same path but `emit_verb`.  `ig` (optional) filters via IGNO.
//  Returns YES via `*any_out` if at least one row matched (caller
//  uses it to decide whether to fall back to a different source).
static ok64 post_expand_under(u8b src, ron60 emit_verb, u8cs prefix,
                              ignocp ig, u8bp out, b8 *any_out) {
    sane(src && out);
    if (any_out) *any_out = NO;
    if (u8bDataLen(src) == 0) done;

    a_dup(u8c, scan, u8bData(src));
    while (!u8csEmpty(scan)) {
        ulogrec rec = {};
        ok64 dr = ULOGu8sDrain(scan, &rec);
        if (dr == NODATA) break;
        if (dr != OK) continue;
        u8cs path = {rec.uri.path[0], rec.uri.path[1]};
        if (!post_path_under(path, prefix)) continue;
        if (ig && IGNOMatch(ig, path, NO)) continue;

        uri u = {};
        u.path[0] = path[0];
        u.path[1] = path[1];
        ulogrec out_rec = {.ts = rec.ts, .verb = emit_verb, .uri = u};
        ok64 fo = ULOGu8sFeed(u8bIdle(out), &out_rec);
        if (fo != OK) return fo;
        if (any_out) *any_out = YES;
    }
    done;
}

//  Per-walk context for `post_pd_cb`.  Holds the two unsorted ULOG
//  intent buffers plus the baseline / wt cursors needed for in-place
//  dir-prefix expansion.  3b2 fully absorbed `post_expand_dir_rows`.
typedef struct {
    post_ctx *c;
    u8bp      put_unsorted;
    u8bp      del_unsorted;
    u8bp      bu;            // baseline ULOG (KEEPTreeULog) — borrowed
    u8bp      wu;            // wt ULOG (SNIFFWtULog) — borrowed
    ignocp    ig;            // wt-root .gitignore
    ron60     v_put_filter;
    ron60     v_del_filter;
    ron60     v_put_emit;
    ron60     v_del_emit;
} pd_walk_ctx;

static ok64 post_pd_cb(ulogreccp src, void *vctx) {
    sane(vctx);
    pd_walk_ctx *w = (pd_walk_ctx *)vctx;
    post_ctx    *c = w->c;
    c->any_pd = YES;

    u8cs path = {src->uri.path[0], src->uri.path[1]};
    ron60 verb = src->verb;
    ron60 ts   = src->ts;

    //  Trailing-slash paths are dir prefixes; expand against bu/wu now.
    //
    //  Rules (preserved from the legacy post_expand_dir_rows):
    //    * delete dir/: emit a delete ULOG row for every baseline path
    //      strictly under the prefix.
    //    * put dir/, baseline coverage exists: emit a put ULOG row per
    //      baseline path under the prefix (IGNO doesn't apply to
    //      tracked files).
    //    * put dir/, no baseline match: emit a put ULOG row per wt
    //      path under the prefix (subject to IGNO).
    if (!$empty(path) && *u8csLast(path) == '/') {
        if (verb == w->v_del_filter) {
            return post_expand_under(w->bu, w->v_del_emit, path,
                                     NULL, w->del_unsorted, NULL);
        }
        if (verb == w->v_put_filter) {
            b8 any_base = NO;
            ok64 br = post_expand_under(w->bu, w->v_put_emit, path,
                                        NULL, w->put_unsorted, &any_base);
            if (br != OK) return br;
            if (any_base) return OK;
            return post_expand_under(w->wu, w->v_put_emit, path,
                                     w->ig, w->put_unsorted, NULL);
        }
        return OK;
    }

    //  Sub-mount bump (`put <sub>#<40-hex>`): fragment is a new
    //  gitlink sha, not a move destination.  Emit one put intent
    //  preserving the URI so post_classify_step can read the new sha.
    if (verb == w->v_put_filter) {
        u8cs frag = {src->uri.fragment[0], src->uri.fragment[1]};
        if (u8csLen(frag) == 40 && HEXu8sValid(frag)) {
            ulogrec rec = {.ts = ts, .verb = w->v_put_emit,
                           .uri = src->uri};
            return ULOGu8sFeed(u8bIdle(w->put_unsorted), &rec);
        }
    }

    //  Move-form put row (`put <old>#<new>`): expand to one del intent
    //  for the source path and one put intent for the dest path.  The
    //  classifier then handles each side via its existing rules
    //  (del + src_base → UNLINK; put + src_wt → ADD).
    if (verb == w->v_put_filter && !u8csEmpty(src->uri.fragment)) {
        u8cs dst = {src->uri.fragment[0], src->uri.fragment[1]};
        uri u_src = {};
        u_src.path[0] = path[0]; u_src.path[1] = path[1];
        ulogrec d_rec = {.ts = ts, .verb = w->v_del_emit, .uri = u_src};
        ok64 ro = ULOGu8sFeed(u8bIdle(w->del_unsorted), &d_rec);
        if (ro != OK) return ro;
        uri u_dst = {};
        u_dst.path[0] = dst[0]; u_dst.path[1] = dst[1];
        ulogrec p_rec = {.ts = ts, .verb = w->v_put_emit, .uri = u_dst};
        return ULOGu8sFeed(u8bIdle(w->put_unsorted), &p_rec);
    }

    //  File-level put/delete row → emit one ULOG line into the
    //  appropriate intent buffer.  Sort+dedup happens after the walk;
    //  the merge classifier then dispatches on `verb` to set
    //  the appropriate intent buffer for the merge.
    uri u = {};
    u.path[0] = path[0];
    u.path[1] = path[1];
    if (verb == w->v_put_filter) {
        ulogrec rec = {.ts = ts, .verb = w->v_put_emit, .uri = u};
        return ULOGu8sFeed(u8bIdle(w->put_unsorted), &rec);
    }
    if (verb == w->v_del_filter) {
        ulogrec rec = {.ts = ts, .verb = w->v_del_emit, .uri = u};
        return ULOGu8sFeed(u8bIdle(w->del_unsorted), &rec);
    }
    return OK;
}

//  Sort+dedup an unsorted ULOG intent buffer into `dst` (lex-by-path).
//  Uses HEAPu8csPopZ over a per-row slice array.  `dst` is reset.
static ok64 post_sort_dedup_intent(u8b src, u8b dst) {
    sane(src && dst);
    u8bReset(dst);
    if (u8bDataLen(src) == 0) done;

    //  Heap of one-line slices into `src`.  Cap is loose — `src` has
    //  no zero-length lines so divide by 16-byte minimum row.
    Bu8cs slices = {};
    size_t cap = u8bDataLen(src) / 16 + 16;
    call(u8csbAllocate, slices, cap);

    a_dup(u8c, scan, u8bData(src));
    while (!u8csEmpty(scan)) {
        a_dup(u8c, rest, scan);
        u8cs line = {scan[0], scan[1]};
        if (u8csFind(rest, '\n') == OK) {
            //  Include the trailing '\n' in the line slice.
            u8csUsed(rest, 1);
            line[1] = rest[0];
        }
        u8csMv(scan, rest);
        ok64 fo = u8csbFeedP(slices, &line);
        if (fo != OK) { u8csbFree(slices); return fo; }
    }

    //  Heap-sort via repeated pop.
    u8cssHeapZ(u8csbData(slices), ULOGu8csZbyUri);

    u8cs prev = {};
    b8   have_prev = NO;
    while (u8csbDataLen(slices) > 0) {
        u8cs cur = {};
        ok64 po = HEAPu8csPopZ(&cur, slices, ULOGu8csZbyUri);
        if (po != OK) break;

        //  Dedup: a == b under ULOGu8csZbyUri iff !(a<b) && !(b<a).
        if (have_prev &&
            !ULOGu8csZbyUri(&prev, &cur) &&
            !ULOGu8csZbyUri(&cur, &prev)) {
            continue;
        }
        a_dup(u8c, cur_dup, cur);
        ok64 fo = u8bFeed(dst, cur_dup);
        if (fo != OK) { u8csbFree(slices); return fo; }
        u8csMv(prev, cur);
        have_prev = YES;
    }

    u8csbFree(slices);
    done;
}

// --- Baseline tree resolution ---

//  Resolve the baseline URI to a tree sha (no walk — the merge below
//  consumes the tree via KEEPTreeListLeaves).  Sets c->has_base as a
//  side-effect.  Skips patch rows via SNIFFAtCurTip so the baseline
//  is the wt's anchor commit, not the latest absorbed patch.  See
//  VERBS.md §POST "Per-file classification via stamps" for the squash
//  rationale (we only need ours as baseline; absorbed patches are
//  mtime-dirty / implicit-dirty).
static ok64 post_resolve_baseline(post_ctx *c, sha1 *root_out, b8 *has_out) {
    sane(c && root_out && has_out);
    ok64 br = SNIFFAtBaselineTreeSha(YES, root_out, has_out);
    if (br == ULOGNONE) { *has_out = NO; done; }
    if (br != OK) return br;
    c->has_base = YES;
    done;
}

// --- Baseline ↔ wt classifier via N-way merge ---

//  Map the verb's bottom RON64 digit (kind suffix appended by
//  KEEPTreeULog / SNIFFWtULog) to its git octal mode.  Unknown
//  letters (or 0 = no suffix) yield 0.
static u16 post_kind_to_mode(u8 kind) {
    switch (kind) {
        case RON_f: return 0100644;
        case RON_x: return 0100755;
        case RON_l: return 0120000;
        case RON_s: return 0160000;
        default:    return 0;
    }
}

//  Inverse of `post_kind_to_mode`: git octal mode → kind letter.
//  Returns 0 for unknown / zero modes (used by unlink rows that
//  carry no meaningful kind, and the tree-build dir entry, neither
//  of which round-trips through the verb).
static u8 post_mode_to_kind(u16 mode) {
    switch (mode) {
        case 0100644: return RON_f;
        case 0100755: return RON_x;
        case 0120000: return RON_l;
        case 0160000: return RON_s;
        default:      return 0;
    }
}

//  Per-step classification context for `post_classify_step`.
typedef struct {
    post_ctx *c;
    ron60     v_base;
    ron60     v_wt;
    ron60     v_put;
    ron60     v_del;
} post_classify_step_ctx;

//  SNIFFMergeWalk step callback.  Inspects the tie group (one record
//  per source verb) for one path, decides the fate, hashes the wt
//  file when the fate is `add`, and emits exactly one decision row
//  into c->decisions.
//
//  Verbs in the output stream: keep / unlink / add (see post_emit_*).
static ok64 post_classify_step(ulogreccp recs, u32 n, void *vctx) {
    post_classify_step_ctx *cctx = (post_classify_step_ctx *)vctx;
    post_ctx *c = cctx->c;

    u8cs path = {recs[0].uri.path[0], recs[0].uri.path[1]};
    if ($empty(path)) return OK;

    //  Sniff-meta paths (.be/wtlog / .be/* / .git*): never carry into
    //  the new commit's tree, even when present in the baseline tree.
    //  Legacy trees that committed these accidentally are scrubbed on
    //  the next post.  Skipping here drops them from the new commit's
    //  tree without touching the on-disk meta files.
    if (SNIFFSkipMeta(path)) return OK;

    //  Inspect sources contributing to this path.  Baseline / wt rows
    //  carry a kind suffix in the verb's bottom RON64 digit (appended
    //  by KEEPTreeULog / SNIFFWtULog), so source dispatch tests the
    //  stem.  Put/delete intent rows carry no suffix — equality match.
    ulogreccp src_base = NULL, src_wt = NULL, src_put = NULL;
    b8 has_put = NO, has_del = NO;
    for (u32 i = 0; i < n; i++) {
        ulogreccp m = &recs[i];
        if      (ok64stem(m->verb) == cctx->v_base) src_base = m;
        else if (ok64stem(m->verb) == cctx->v_wt)   src_wt   = m;
        else if (m->verb == cctx->v_put)          { has_put = YES; src_put = m; }
        else if (m->verb == cctx->v_del)            has_del  = YES;
    }

    //  Pull baseline mode (from verb) + sha (from fragment) when present.
    u16 base_mode = 0;
    sha1 base_sha = {};
    if (src_base) {
        base_mode = post_kind_to_mode(ok64Lit(src_base->verb, 0));
        a_raw(bin_s, base_sha);
        a_dup(u8c, frag_dup, src_base->uri.fragment);
        HEXu8sDrainSome(bin_s, frag_dup);
    }
    //  Pull wt mode (from verb) when on disk.
    u16 wt_mode = 0;
    if (src_wt) wt_mode = post_kind_to_mode(ok64Lit(src_wt->verb, 0));

    //  --- Decision ladder (mirrors the old post_decide) ---

    //  Gitlink: carry through verbatim — no on-disk file expected.
    //  Sub-mount bump (`put <sub>#<40-hex>`) overrides the keep with
    //  an ADD at the new sha so the new tree records the bumped pin.
    if (base_mode == 0160000) {
        if (src_put) {
            u8cs put_frag = {src_put->uri.fragment[0],
                             src_put->uri.fragment[1]};
            if (u8csLen(put_frag) == 40 && HEXu8sValid(put_frag)) {
                sha1 new_sha = {};
                a_raw(bin_s, new_sha);
                a_dup(u8c, frag_dup, put_frag);
                if (HEXu8sDrainSome(bin_s, frag_dup) == OK &&
                    !sha1Eq(&new_sha, &base_sha)) {
                    return post_emit_decision(c, POST_V_ADD, path,
                                              base_mode,
                                              &base_sha, &new_sha);
                }
            }
        }
        return post_emit_decision(c, POST_V_KEEP, path, base_mode,
                                  NULL, &base_sha);
    }

    //  Explicit delete row: drop unconditionally.  Unlink iff the
    //  path was tracked or currently exists on disk.
    if (has_del) {
        if (src_base || src_wt) {
            return post_emit_decision(c, POST_V_UNLINK, path,
                                      0, NULL, NULL);
        }
        return OK;
    }

    //  Explicit put row.
    if (has_put) {
        //  NEW gitlink (no baseline tree entry, no on-disk file) where
        //  the put row carries a 40-hex sha → submodule-add.  Emit an
        //  ADD with mode 0160000 so the new tree records the gitlink.
        //  Plan §POST step 3: parent commits a fresh `vendor/sub`
        //  entry alongside the synthesised `.gitmodules` blob.
        if (!src_base && !src_wt && src_put) {
            u8cs put_frag = {src_put->uri.fragment[0],
                             src_put->uri.fragment[1]};
            if (u8csLen(put_frag) == 40 && HEXu8sValid(put_frag)) {
                sha1 new_sha = {};
                a_raw(bin_s, new_sha);
                a_dup(u8c, frag_dup, put_frag);
                if (HEXu8sDrainSome(bin_s, frag_dup) == OK) {
                    return post_emit_decision(c, POST_V_ADD, path,
                                              0160000, NULL, &new_sha);
                }
            }
        }
        if (!src_wt) {
            //  Explicit put of a missing file: drop, unlink if tracked.
            if (src_base) {
                return post_emit_decision(c, POST_V_UNLINK, path,
                                          0, NULL, NULL);
            }
            return OK;
        }
        sha1 new_sha = {};
        if (post_hash_path(post_reporoot(), path, wt_mode, &new_sha) != OK)
            return OK;
        sha1cp old = src_base ? &base_sha : NULL;
        return post_emit_decision(c, POST_V_ADD, path, wt_mode,
                                  old, &new_sha);
    }

    //  No explicit rule.  Branches by (in baseline?) × (on disk?).

    //  Missing from wt.
    if (!src_wt) {
        //  Gitignored baseline files: keep verbatim — we don't see
        //  them on disk because SNIFFWtULog filters via SNIFFSkipMeta.
        if (src_base && SNIFFSkipMeta(path)) {
            return post_emit_decision(c, POST_V_KEEP, path, base_mode,
                                      NULL, &base_sha);
        }
        if (c->any_pd) {
            //  Selective mode: keep baseline entries unchanged.
            if (src_base) {
                return post_emit_decision(c, POST_V_KEEP, path, base_mode,
                                          NULL, &base_sha);
            }
            return OK;
        }
        //  Implicit mode: missing tracked file is a deletion.
        if (src_base) {
            return post_emit_decision(c, POST_V_UNLINK, path,
                                      0, NULL, NULL);
        }
        return OK;
    }

    //  On disk, no explicit rule.  Untracked + selective = ignore.
    if (!src_base && c->any_pd) return OK;

    a_path(fp);
    if (SNIFFFullpath(fp, post_reporoot(), path) != OK) return OK;
    filestat fs = {};
    ok64 lo = FILELStat(&fs, $path(fp));
    if (lo == FILENOENT) {
        if (src_base) {
            return post_emit_decision(c, POST_V_UNLINK, path,
                                      0, NULL, NULL);
        }
        return OK;
    }
    if (lo != OK) return lo;
    ron60 mtime_r = fs.mtime;

    if (SNIFFAtKnown(mtime_r)) {
        //  Per-file stamp lookup: who owns this mtime?
        //    get/post → KEEP (baseline-clean)
        //    patch/put/mod → REWRITE (current bytes; patch row's
        //                    `theirs` joins parents via patch_parents)
        ron60 ow_verb = 0;
        uri ow_u = {};
        ok64 ro = SNIFFAtRowAtTs(mtime_r, &ow_verb, &ow_u);
        if (ro == OK) {
            ron60 vg = SNIFFAtVerbGet();
            ron60 vp = SNIFFAtVerbPost();
            if (ow_verb == vg || ow_verb == vp) {
                if (src_base) {
                    return post_emit_decision(c, POST_V_KEEP, path,
                                              base_mode, NULL,
                                              &base_sha);
                }
                return OK;  // untracked clean — shouldn't happen
            }
            //  patch / put / mod stamp owns this mtime → add.
            sha1 new_sha = {};
            if (post_hash_path(post_reporoot(), path, wt_mode, &new_sha) != OK)
                return OK;
            sha1cp old = src_base ? &base_sha : NULL;
            return post_emit_decision(c, POST_V_ADD, path, wt_mode,
                                      old, &new_sha);
        }
        //  ts known but row not found (corrupt log?) — fallback keep.
        if (src_base) {
            return post_emit_decision(c, POST_V_KEEP, path, base_mode,
                                      NULL, &base_sha);
        }
        return OK;
    }

    //  mtime unknown.
    if (src_base) {
        //  Tracked + dirty.  In selective mode (any explicit put/delete
        //  in scope) we ignore — only files named by a put row land in
        //  the commit, plus deletes drop their targets.  Implicit mode
        //  (commit-all): hash and rewrite.
        if (c->any_pd) {
            return post_emit_decision(c, POST_V_KEEP, path, base_mode,
                                      NULL, &base_sha);
        }
        sha1 disk_sha = {};
        if (post_hash_path(post_reporoot(), path, wt_mode, &disk_sha) != OK)
            return OK;
        if (sha1Eq(&disk_sha, &base_sha)) {
            //  Identical → KEEP (mtime drifted but bytes match).
            //  Re-stamp with this POST's stamp_ts so the file aligns
            //  with the new post row in `.be/wtlog`; the next bare `be`
            //  fast-paths it via SNIFFAtKnown without re-hashing.
            //  Mirrors PUT's bare-walk re-stamp at put_visit_tracked.
            a_path(fp);
            if (SNIFFFullpath(fp, post_reporoot(), path) == OK)
                (void)SNIFFAtStampPath(fp, c->stamp_ts);
            return post_emit_decision(c, POST_V_KEEP, path, base_mode,
                                      NULL, &base_sha);
        }
        return post_emit_decision(c, POST_V_ADD, path, wt_mode,
                                  &base_sha, &disk_sha);
    }
    if (!c->has_base) {
        //  Fresh-repo first commit: auto-stage every dirty file.
        sha1 new_sha = {};
        if (post_hash_path(post_reporoot(), path, wt_mode, &new_sha) != OK)
            return OK;
        return post_emit_decision(c, POST_V_ADD, path, wt_mode,
                                  NULL, &new_sha);
    }
    //  Untracked + dirty + has-base.  Per VERBS.md §POST: implicit
    //  mode commits all dirty *tracked* files; an untracked sibling
    //  must be explicitly `be put`-staged to land in the next commit.
    //  Same in selective mode.  Either way, ignore here.
    return OK;
}

//  Drive `flag[idx]` / `rec[idx]` population from a 4-way ULOG-row
//  merge over (baseline tree, wt, sorted ULOG put intents, sorted ULOG
//  delete intents).  All four buffers are caller-owned — `bu` and `wu`
//  are also reused by `post_pd_cb` for dir-prefix expansion.  Each
//  input carries a distinct verb so the step callback can dispatch
//  per record.
static ok64 post_classify_via_merge(post_ctx *c,
                                    u8b bu, u8b wu,
                                    u8b put_buf, u8b del_buf,
                                    ron60 v_base, ron60 v_wt,
                                    ron60 v_put,  ron60 v_del) {
    sane(c);

    //  Heap-walk all 4 cursors; dispatch per row in `post_classify_step`.
    a_dup(u8c, view_b, u8bData(bu));
    a_dup(u8c, view_w, u8bData(wu));
    a_dup(u8c, view_p, u8bData(put_buf));
    a_dup(u8c, view_d, u8bData(del_buf));
    a_pad(u8cs, ins, 4);
    u8cssFeed1(ins_idle, view_b);
    u8cssFeed1(ins_idle, view_w);
    u8cssFeed1(ins_idle, view_p);
    u8cssFeed1(ins_idle, view_d);
    a_dup(u8cs, cursors, u8csbData(ins));

    post_classify_step_ctx cctx = {
        .c = c, .v_base = v_base, .v_wt = v_wt,
        .v_put = v_put, .v_del = v_del,
    };
    return SNIFFMergeWalk(cursors, post_classify_step, &cctx);
}

// --- Tree building (bottom-up from sorted paths) ---

//  Decode the optional old-sha (40 hex) carried in an `add` row's
//  query for the modify-vs-add distinction.  Returns YES + fills
//  `*out` on success; NO if the query is absent or wrong-length.
static b8 post_decision_old_sha(uricp u, sha1 *out) {
    if (!u || !out) return NO;
    if (u8csLen(u->query) != 40) return NO;
    a_raw(bin, *out);
    a_dup(u8c, hex_dup, u->query);
    return HEXu8sDrainSome(bin, hex_dup) == OK;
}

//  YES iff `path` starts with `prefix`.  Empty prefix matches all.
fun b8 post_path_under_prefix(u8cs path, u8cs prefix) {
    if (u8csLen(prefix) == 0) return YES;
    return u8csHasPrefix(path, prefix);
}

//  Peek the path of the next decisions row without advancing scan.
//  Returns YES + fills `*out` on success; NO when scan is empty or
//  malformed (caller treats as end).
static b8 post_peek_path(u8cs scan_in, u8cs out) {
    if (u8csEmpty(scan_in)) return NO;
    a_dup(u8c, scan, scan_in);
    ulogrec rec = {};
    if (ULOGu8sDrain(scan, &rec) != OK) return NO;
    out[0] = rec.uri.path[0];
    out[1] = rec.uri.path[1];
    return YES;
}

//  Recursively build a git tree object for all decision rows in
//  `subslice` (a contiguous u8cs over `c->decisions` covering rows
//  whose path starts with `prefix`).  Decisions are lex-sorted by
//  path, so subdir rows form contiguous sub-ranges sliced off here.
//  Emits `(u32 length, body bytes)` records into `tree_body_list`
//  for later pack-time replay; sets `*tree_out` to the tree's sha.
//  Empty subslice → `*tree_out` zeroed, no body emitted.
static ok64 post_build_tree(u8cs subslice, u8cs prefix,
                            sha1 *tree_out, Bu8 tree_body_list,
                            u32 *emit_count) {
    sane(tree_out);

    a_carve(u8, tree, $len(subslice) + 256);

    a_dup(u8c, scan, subslice);
    while (!u8csEmpty(scan)) {
        //  Snapshot scan before draining the next row — when we
        //  detect a subdir entry below we rewind to this position so
        //  the recursive subslice covers it from the start.
        a_dup(u8c, scan_snap, scan);
        ulogrec rec = {};
        ok64 dr = ULOGu8sDrain(scan, &rec);
        if (dr == NODATA) break;
        if (dr != OK) continue;

        //  Skip unlink rows (no tree entry).  Defensive against any
        //  out-of-prefix row sneaking in (subslice should preclude it).
        if (rec.verb == POST_V_UNLINK) continue;
        ron60 stem = ok64stem(rec.verb);
        if (stem != POST_V_KEEP && stem != POST_V_ADD) continue;
        if (!post_path_under_prefix(rec.uri.path, prefix)) continue;

        u8cs path = {rec.uri.path[0], rec.uri.path[1]};
        size_t plen = $len(prefix);
        u8cs rest = {$atp(path, plen), path[1]};
        if ($empty(rest)) continue;

        //  Locate the first '/' in `rest` to distinguish a direct
        //  child file from an entry in a deeper subtree.
        a_dup(u8c, rest_scan, rest);
        b8 has_slash = (u8csFind(rest_scan, '/') == OK);

        if (has_slash) {
            //  Subdir entry.  Build subprefix = prefix + dirname + '/',
            //  carve off the contiguous range of decision rows under
            //  it from the parent subslice, and recurse.
            u8cs dirname = {rest[0], rest_scan[0]};
            a_pad(u8, subprefix_buf, 2048);
            u8bFeed(subprefix_buf, prefix);
            u8bFeed(subprefix_buf, dirname);
            u8bFeed1(subprefix_buf, '/');
            a_dup(u8c, subprefix, u8bDataC(subprefix_buf));

            //  Rewind scan to the start of this subdir's first row, then
            //  advance until a row's path leaves `subprefix`.
            u8csMv(scan, scan_snap);
            a_dup(u8c, sub_subslice, scan);
            while (!u8csEmpty(scan)) {
                u8cs peek = {};
                if (!post_peek_path(scan, peek)) break;
                if (!post_path_under_prefix(peek, subprefix)) break;
                ulogrec drop = {};
                if (ULOGu8sDrain(scan, &drop) != OK) break;
            }
            sub_subslice[1] = scan[0];

            sha1 sub_sha = {};
            call(post_build_tree, sub_subslice, subprefix,
                                  &sub_sha, tree_body_list, emit_count);

            if (!sha1empty(&sub_sha)) {
                post_mode_feed(tree, 040000);
                u8bFeed1(tree, ' ');
                u8bFeed(tree, dirname);
                u8bFeed1(tree, 0);
                a_rawc(sr, sub_sha);
                u8bFeed(tree, sr);
            }
            continue;
        }

        //  Direct-child file entry.  Mode is the kind suffix in the
        //  verb's bottom RON64 digit; default to 100644 if absent.
        sha1 entry_sha = {};
        if (u8csLen(rec.uri.fragment) == 40) {
            a_raw(bin_s, entry_sha);
            a_dup(u8c, frag_dup, rec.uri.fragment);
            HEXu8sDrainSome(bin_s, frag_dup);
        }
        if (sha1empty(&entry_sha)) continue;

        u16 mode = post_kind_to_mode(ok64Lit(rec.verb, 0));
        if (mode == 0) mode = 0100644;

        post_mode_feed(tree, mode);
        u8bFeed1(tree, ' ');
        u8bFeed(tree, rest);
        u8bFeed1(tree, 0);
        a_rawc(er, entry_sha);
        u8bFeed(tree, er);
    }

    if (u8bDataLen(tree) == 0) {
        zerop(tree_out);
        done;
    }

    KEEPObjSha(tree_out, DOG_OBJ_TREE, u8bDataC(tree));

    //  Record (len u32, body bytes) in tree_body_list; POSTCommit
    //  replays them later to feed keeper in commit→trees→blobs order.
    u32 tlen = (u32)u8bDataLen(tree);
    u8cs tl = {(u8cp)&tlen, (u8cp)&tlen + sizeof(u32)};
    u8bFeed(tree_body_list, tl);
    u8bFeed(tree_body_list, u8bDataC(tree));
    (*emit_count)++;

    done;
}

// --- Empty-tree feed (handles "no files to commit" case) ---

static ok64 post_feed_empty_tree(keeper *k, keep_pack *p, sha1 *out) {
    u8cs empty = {};
    return KEEPPackFeed(p, DOG_OBJ_TREE, empty, 0, out);
}

// --- Resolve parent commit sha for the commit body ---

static ok64 post_parent_sha(keeper *k, u8csc parent_hex, sha1 *out) {
    sane(out && $ok(parent_hex));
    if ($len(parent_hex) != 40) fail(SNIFFFAIL);

    a_dup(u8c, hx, parent_hex);
    a_raw(bin, *out);
    HEXu8sDrainSome(bin, hx);
    //  Verify the commit actually lives in keeper (sanity check).
    a_carve(u8, tmp, 1UL << 20);
    u8 ctype = 0;
    ok64 go = KEEPGetExact(out, tmp, &ctype);
    if (go != OK || ctype != DOG_OBJ_COMMIT) fail(SNIFFFAIL);
    done;
}

// --- Baseline branch query (from ULOG) ---

//  Read the latest baseline (`get`/`post`/`patch`) row.  Fills `out`
//  with the be-branch path (row's query), and `*parent_out` with the
//  current commit's sha (row's `#fragment`).  `*had_baseline_out` is
//  YES iff a baseline row exists.
//
//  Single-parent everywhere on the write path: after the PATCH rewrite
//  the baseline query no longer chains `&<theirs>` SHAs; one ours sha
//  is the only parent the new commit gets.
//  Detached-wt detector — mirror of PATCH.c's is_detached_wt (DIS-009).
//  Detached iff the cur-tip row is `?<sha>` (40-hex query, EMPTY
//  fragment): GET writes this for `be get ?<sha>` / bare `be get <sha>`.
//  The attached branch form `?<branch>#<sha>` (query-side ref) and the
//  trunk-state form `?#<sha>` (empty query = trunk, sha in fragment)
//  both carry either a query ref or a non-empty fragment, so neither
//  trips the gate — POST commits legitimately in those cases.  POST
//  refuses on a detached wt: there is no branch to record the commit
//  against (would silently graft onto trunk/empty branch).
static b8 post_is_detached_wt(void) {
    ron60 ts = 0, verb = 0;
    uri u = {};
    if (SNIFFAtCurTip(&ts, &verb, &u) != OK) return NO;
    if (!u8csEmpty(u.fragment)) return NO;
    a_dup(u8c, q, u.query);
    if (u8csLen(q) != 40) return NO;
    return HEXu8sValid(q);
}

static ok64 post_collect_parents(u8bp out, sha1 *parent_out, b8 *has_parent_out,
                                 b8 *had_baseline_out) {
    sane(out && parent_out && has_parent_out && had_baseline_out);
    u8bReset(out);
    *has_parent_out = NO;
    *had_baseline_out = NO;
    ron60 ts = 0, verb = 0;
    uri u = {};
    ok64 r = SNIFFAtCurTip(&ts, &verb, &u);
    if (r == ULOGNONE) done;        // fresh repo — root commit allowed
    if (r != OK) return r;
    *had_baseline_out = YES;

    //  Current commit (`ours`) lives in the row's `#fragment` (canonical
    //  form); legacy rows kept it in the query, which `SNIFFAtQueryFirstSha`
    //  handles transparently.
    {
        sha1hex hex = {};
        if (SNIFFAtQueryFirstSha(&u, &hex) == OK) {
            sha1 ph = {};
            if (sha1FromSha1hex(&ph, &hex) == OK) {
                *parent_out = ph;
                *has_parent_out = YES;
            }
        }
    }

    //  Walk the query for the branch (first non-sha chunk).
    //  Single-parent invariant: any extra SHAs in the query are
    //  legacy artefacts from pre-rewrite PATCH rows; they're ignored.
    u8cs branch = {};
    DOGQueryBranchOnly(u.query, branch);
    if (!u8csEmpty(branch)) u8bFeed(out, branch);
    done;
}

//  Single-parent invariant: the multi-parent injection helpers
//  `post_patch_theirs` / `post_add_patch_parents` were removed when
//  PATCH stopped chaining `&<theirs>` onto baseline.  See VERBS.md
//  §POST: parents = [ours] only on the write path.

// --- Shared scan: produce the change-set into a post_ctx ---
//
//  Steps 2..5 of POSTCommit, lifted so a dry-run print path can run
//  the same scan without committing.  Caller pre-fills `post_reporoot()`,
//  `c->k`, `c->cap`, `c->rec`, `c->flag`, `c->last_post_ts`; this
//  function drives the baseline walk + put/delete scan + wt scan +
//  dir-row expansion + per-path decide.  On return, `c->flag[idx]`
//  carries the keep/unlink/add decision per path in `c->decisions`,
//  and `*base_tree_sha` / `*have_base` reflect the baseline tree (if
//  any) so the caller can reuse them for tree-build.
static ok64 post_scan_changeset(post_ctx *c, sha1 *base_tree_sha,
                                b8 *have_base) {
    sane(c && base_tree_sha && have_base);

    //  2. Resolve baseline URI → tree sha (no walk yet).
    call(post_resolve_baseline, c, base_tree_sha, have_base);

    //  3. Build the baseline + wt ULOG row buffers up-front.  Both are
    //     reused by post_pd_cb (dir-prefix expansion) and the merge
    //     classifier; sharing avoids two scans of the same trees.
    a_cstr(s_basev, "base"); a_dup(u8c, dbv, s_basev);
    a_cstr(s_wtv,   "wt");   a_dup(u8c, dwv, s_wtv);
    a_cstr(s_putv,  "put");  a_dup(u8c, dpv, s_putv);
    a_cstr(s_delv,  "del");  a_dup(u8c, ddv, s_delv);
    ron60 v_base = 0, v_wt = 0, v_put_emit = 0, v_del_emit = 0;
    call(RONutf8sDrain, &v_base,     dbv);
    call(RONutf8sDrain, &v_wt,       dwv);
    call(RONutf8sDrain, &v_put_emit, dpv);
    call(RONutf8sDrain, &v_del_emit, ddv);

    //  Sized for real-world wt: a single rsync-then-`be put .` over a
    //  freshly checked-out git tag (~5k tracked files) easily fills
    //  more than 64 KB of put-rows; bu/wu used to overflow on wt's
    //  with > ~7k files at ~140 B/row.  4 MB for put/del, 16 MB for
    //  bu/wu — all BASS-carved, rewound when this function returns.
    a_carve(u8, bu,           1UL << 24);
    a_carve(u8, wu,           1UL << 24);
    a_carve(u8, put_unsorted, 1UL << 22);
    a_carve(u8, del_unsorted, 1UL << 22);
    a_carve(u8, put_buf,      1UL << 22);
    a_carve(u8, del_buf,      1UL << 22);

    if (*have_base) call(KEEPTreeULog, base_tree_sha->data, 0, v_base, bu);
    call(SNIFFWtULog, post_reporoot(), v_wt, wu);

    //  4. Put/delete scan since last post.  File-level rows go into
    //     the unsorted intent buffers; dir-prefix rows are expanded
    //     in-line against bu / wu via post_expand_under.
    pd_walk_ctx walk = {
        .c = c,
        .put_unsorted = put_unsorted,
        .del_unsorted = del_unsorted,
        .bu           = bu,
        .wu           = wu,
        .ig           = &SNIFF.ignores,
        .v_put_filter = SNIFFAtVerbPut(),
        .v_del_filter = SNIFFAtVerbDelete(),
        .v_put_emit   = v_put_emit,
        .v_del_emit   = v_del_emit,
    };
    call(SNIFFAtScanPutDelete, c->last_post_ts, post_pd_cb, &walk);
    call(post_sort_dedup_intent, put_unsorted, put_buf);
    call(post_sort_dedup_intent, del_unsorted, del_buf);

    //  5. Classify baseline + wt + put/del intents via 4-way merge,
    //     emitting one keep/unlink/add decision row per distinct path
    //     into ctx.decisions.
    call(post_classify_via_merge, c, bu, wu, put_buf, del_buf,
                                  v_base, v_wt, v_put_emit, v_del_emit);

    //  classify_step inlined the per-path decide+resolve+hash and
    //  emitted one decision row per distinct path.  Downstream
    //  consumers drain c->decisions.
    done;
}

//  Initialise the post_ctx for a scan and stamp the per-commit ts.
//  Keeper + reporoot are read from the SNIFF / KEEP singletons; no
//  need to plumb them through.  `decisions` is a BASS-carved buffer
//  owned by the caller's frame (so it survives this init's call()
//  rewind); we only copy its four boundary pointers into the ctx.
static ok64 post_ctx_init(post_ctx *c, u8bp decisions) {
    sane(c && decisions);
    *c = (post_ctx){
        .last_post_ts = SNIFFAtLastPostTs(),
    };

    //  Decisions buffer holds the full per-commit ULOG-row stream.
    ((u8 **)c->decisions)[0] = decisions[0];
    ((u8 **)c->decisions)[1] = decisions[1];
    ((u8 **)c->decisions)[2] = decisions[2];
    ((u8 **)c->decisions)[3] = decisions[3];

    //  Single per-commit stamp ts; carried in every decision row,
    //  used to re-stamp content-clean drifted files (see
    //  `post_classify_step`), and as the wall-clock seconds in the
    //  commit body's author/committer headers.
    SNIFFAtNow(&c->stamp_ts, &c->stamp_tv);
    done;
}

//  Emit one decision ULOG row into c->decisions.
//
//  Row shapes:
//    keep<k>   : <ts>\tkeep<k>\t<path>#<old_sha>\n
//    add<k>    : <ts>\tadd<k>\t<path>[?<old_sha>]#<new_sha>\n
//                (optional ?<old_sha> in query is present iff the path
//                had a baseline entry — distinguishes "M" from "A".)
//    unlink    : <ts>\tunlink\t<path>\n
//
//  `<k>` is the RON64 kind letter encoding the git mode (f/x/l/s);
//  appended via `ok64sub` so `ok64stem(verb)` recovers the bare
//  POST_V_KEEP / POST_V_ADD stem.  Unlink rows carry no suffix.
static ok64 post_emit_decision(post_ctx *c, ron60 verb_stem,
                               u8cs path, u16 mode,
                               sha1cp old_sha, sha1cp frag_sha) {
    sane(c && verb_stem);

    //  Append the kind letter to the verb stem when we have a real
    //  mode.  Unlink rows pass mode==0 and stay as bare POST_V_UNLINK.
    ron60 verb = verb_stem;
    u8 kletter = post_mode_to_kind(mode);
    if (kletter != 0) verb = ok64sub(verb_stem, kletter);

    //  query: the optional old-sha (40 hex), present only when the
    //  caller passed a non-empty `old_sha` (i.e. modified-add row).
    a_pad(u8, query_buf, 40);
    if (old_sha && !sha1empty(old_sha)) {
        a_rawc(bin, *old_sha);
        HEXu8sFeedSome(u8bIdle(query_buf), bin);
    }

    //  fragment: the row's primary sha (baseline-sha for keep,
    //  new-sha for add, empty for unlink).
    a_pad(u8, hex_buf, 40);
    if (frag_sha && !sha1empty(frag_sha)) {
        a_rawc(bin, *frag_sha);
        HEXu8sFeedSome(hex_buf_idle, bin);
    }

    uri u = {};
    u8csMv(u.path, path);
    if (u8bDataLen(query_buf) > 0)
        u8csMv(u.query, u8bDataC(query_buf));
    if (u8bDataLen(hex_buf) > 0)
        u8csMv(u.fragment, u8bDataC(hex_buf));

    ulogrec rec = {.ts = c->stamp_ts, .verb = verb, .uri = u};
    return ULOGu8sFeed(u8bIdle(c->decisions), &rec);
}

//  Iterate the decisions buffer, invoking `cb` for every row whose
//  verb is in `verb_mask` (bitmask: 1=keep, 2=unlink, 4=add).  cb sees
//  the parsed ulogrec and can pull path/mode/sha from rec->uri.
//  Verb-mask bits for `post_walk_decisions`.
#define POST_VM_KEEP   1u
#define POST_VM_UNLINK 2u
#define POST_VM_ADD    4u
#define POST_VM_ALL    (POST_VM_KEEP | POST_VM_UNLINK | POST_VM_ADD)

typedef ok64 (*post_decision_cb)(post_ctx *c, ulogreccp rec, void *ctx);
static ok64 post_walk_decisions(post_ctx *c, u32 verb_mask,
                                post_decision_cb cb, void *cbctx) {
    sane(c && cb);
    a_dup(u8c, scan, u8bData(c->decisions));
    while (!u8csEmpty(scan)) {
        ulogrec rec = {};
        ok64 dr = ULOGu8sDrain(scan, &rec);
        if (dr == NODATA) break;
        if (dr != OK) continue;
        //  keep/add carry a kind suffix in the verb's bottom RON64
        //  digit; unlink is the bare stem.  Match accordingly.
        u32 bit = 0;
        if      (rec.verb == POST_V_UNLINK)         bit = POST_VM_UNLINK;
        else if (ok64stem(rec.verb) == POST_V_KEEP) bit = POST_VM_KEEP;
        else if (ok64stem(rec.verb) == POST_V_ADD)  bit = POST_VM_ADD;
        if (!(verb_mask & bit)) continue;
        ok64 cr = cb(c, &rec, cbctx);
        if (cr != OK) return cr;
    }
    done;
}

// --- Decision-walk callbacks for the simple per-row loops ---

//  Unlink the wt file.  Errors swallowed — best-effort.
static ok64 post_drain_unlink_cb(post_ctx *c, ulogreccp rec, void *vctx) {
    (void)vctx;
    u8cs path = {rec->uri.path[0], rec->uri.path[1]};
    a_path(fp);
    if (SNIFFFullpath(fp, post_reporoot(), path) != OK) return OK;
    (void)FILEUnLink($path(fp));
    return OK;
}

//  Stamp the wt file with the post ts (only `add` rows reach here).
static ok64 post_drain_stamp_cb(post_ctx *c, ulogreccp rec, void *vctx) {
    (void)vctx;
    u8cs path = {rec->uri.path[0], rec->uri.path[1]};
    a_path(fp);
    if (SNIFFFullpath(fp, post_reporoot(), path) != OK) return OK;
    (void)SNIFFAtStampPath(fp, rec->ts);
    return OK;
}

//  Per-file change printer.  Emits the same ULOG status line as
//  GET/PATCH/push (`<date>\t<verb>\t<path>`, palette-coloured via
//  HUNKMode) rather than the old single-letter `M/A/D` shape — verbs
//  `add` (new file), `mod` (rewrite), `del` (unlink), all in the
//  dog/ULOG.c palette.  `fd` is the sink: STDOUT for the dry-run
//  status, STDERR for the commit-time report.
con ron60 POST_DISP_MOD = 0x31ce8;   // "mod" — rewrite of a tracked file
con ron60 POST_DISP_DEL = 0x28a70;   // "del" — unlink  (add reuses POST_V_ADD)

typedef struct {
    int fd;
    u32 changed;
} post_mad_ctx;

static ok64 post_drain_mad_cb(post_ctx *c, ulogreccp rec, void *vctx) {
    (void)c;
    post_mad_ctx *m = (post_mad_ctx *)vctx;
    ron60 verb = 0;
    if (rec->verb == POST_V_UNLINK)   verb = POST_DISP_DEL;
    else if (ok64stem(rec->verb) == POST_V_ADD) {
        sha1 old = {};
        verb = post_decision_old_sha(&rec->uri, &old) ? POST_DISP_MOD
                                                      : POST_V_ADD;
    }
    if (verb == 0) return OK;
    ulogrec rep = {.ts = 0, .verb = verb};
    u8csMv(rep.uri.path, rec->uri.path);
    a_pad(u8, ub,   1024);
    a_pad(u8, line, 4096);
    hunk hk = {};
    if (ULOGToHunk(&rep, &hk, ub) == OK &&
        HUNKu8sFeedOut(u8bIdle(line), &hk) == OK) {
        a_dup(u8c, out, u8bDataC(line));
        (void)FILEFeedAll(m->fd, out);
    }
    m->changed++;
    return OK;
}

// --- Rebase emit pipeline (Stage 2 phase-2 promote) ---
//
//  When POSTCommit detects a non-ff against the same branch (REFS tip
//  diverges from cur's parent), it builds the new commit normally and
//  then replays it onto the live REFS tip via GRAFRebase.  The rebase
//  callback funnels every emitted (type, sha, body) tuple straight into
//  the active keeper pack so persistence is automatic.  The last commit
//  emit's sha becomes the new tip.
//
//  Atomicity: rebase runs after the first KEEPPackClose, in a second
//  pack opened just for the replay.  GRAFCNFL aborts mid-emit; the
//  caller closes the partial pack (orphan objects are harmless — they
//  are not referenced by REFS) and surfaces GRAFCNFL.  Cascade rebase
//  for descendants is not yet wired; see TODO(spec) at the call site.

//  Rebase emit context.  No `keeper *` — keeper is a singleton; the
//  only state worth carrying is the open pack and the most recent
//  emitted commit's sha.
typedef struct {
    keep_pack *p;
    sha1       last_commit_sha;   //  most recent emitted commit
    b8         have_last_commit;
} post_rebase_ctx;

static ok64 post_rebase_emit_cb(void *vctx, u8 obj_type,
                                sha1cp sha, u8csc body) {
    sane(vctx && sha);
    post_rebase_ctx *rc = (post_rebase_ctx *)vctx;
    sha1 fed = {};
    ok64 fo = KEEPPackFeed(rc->p, obj_type, body, 0, &fed);
    if (fo != OK) return fo;
    if (obj_type == DOG_OBJ_COMMIT) {
        //  Record the last commit sha — that's our rebased tip.
        rc->last_commit_sha = *sha;
        rc->have_last_commit = YES;
        //  Close + reopen the pack so the just-emitted objects become
        //  visible to KEEPGetExact for the next rebase iteration.
        //  GRAFRebase's loop fetches the previous-emit's commit/tree/
        //  blob bodies on the next pass; without this checkpoint they
        //  sit in a booked-but-unindexed pack and aren't resolvable.
        ok64 cl = KEEPPackClose(rc->p);
        if (cl != OK) return cl;
        zerop(rc->p);
        ok64 op = KEEPPackOpen(rc->p);
        if (op != OK) return op;
        rc->p->strict_order = NO;
    }
    return OK;
}

// --- Cascade rebase (Stage 2c) ---
//
//  When phase-2 rewrites a branch's stack (rebase, not ff), every
//  descendant branch that forked off the rewritten tip needs its own
//  stack replayed onto the new tip.  The cascade walker enumerates
//  direct subdirs of `<store>/<branch>/` with a `refs` file and runs
//  GRAFRebase on each, depth-first.  All emits land in the open pack
//  passed by the caller; descendants' new tips are committed via
//  REFSCompareAndAppend top-down.
//
//  Atomicity: this walker stages everything in the pack first; the
//  REFS writes happen after.  On GRAFCNFL we surface the error and
//  leave orphan objects in the pack (REFS unchanged ⇒ unreachable).
//  REFS persistence is best-effort: a CAS race mid-cascade leaves
//  earlier descendants advanced — documented as a known limitation.

//  Resolve a branch's REFS tip (`?<branch>`) to a 20-byte sha.  Returns
//  REFSNONE when no row exists, OK when a tip is present and decoded.
//
//  Internal — NOT routed through KEEPResolveRef.  Callers in this file
//  pass `branch` as the literal REFS key (e.g. `feat/` with trailing
//  slash for the basename-reuse semantic), and the higher-level
//  POSTPromote logic distinguishes `?feat` from `?feat/`.  Going via
//  KEEPResolveRef's dog/QURY normaliser would collapse those.
ok64 POSTResolveBranchTip(sha1 *out, u8cs branch) {
    sane(out);
    a_path(keepdir);
    call(HOMEBranchDir, SNIFF.h, keepdir, NULL);
    a_pad(u8, keybuf, 256);
    u8bFeed1(keybuf, '?');
    if (!u8csEmpty(branch)) u8bFeed(keybuf, branch);
    a_dup(u8c, refkey, u8bData(keybuf));

    a_pad(u8, arena, 1024);
    uri resolved = {};
    ok64 ro = REFSResolve(&resolved, arena, $path(keepdir), refkey);
    if (ro != OK) return ro;
    if ($empty(resolved.query)) return REFSNONE;
    a_dup(u8c, tip_hex, resolved.query);
    if (!u8csEmpty(tip_hex) && *tip_hex[0] == '?') u8csUsed(tip_hex, 1);
    if ($len(tip_hex) != 40) return REFSBAD;
    a_raw(bin, *out);
    a_dup(u8c, hx, tip_hex);
    return HEXu8sDrainSome(bin, hx);
}

//  Cascade record: one descendant branch awaiting its REFS write.
typedef struct {
    u8cs branch;             // canonical absolute path; lives in cascade_ctx.arena
    sha1 old_tip;
    sha1 new_tip;
} cascade_rec;

#define CASCADE_MAX 64

//  Cascade walker context.  Like post_ctx, no `keeper *` or
//  `reporoot` — both reachable as singletons (KEEP, SNIFF.h).
typedef struct {
    keep_pack   *p;
    cascade_rec  recs[CASCADE_MAX];
    u32          n;
    ok64         err;
    //  Optional: branch path to skip during the walk (cross-branch
    //  promote uses this so cur is not double-rebased — auto-sync
    //  handles cur directly).  Empty disables the filter.
    u8cs         skip;
    Bu8          arena;     // backs each rec's branch slice
} cascade_ctx;

//  Stage one descendant: compute its old fork point relative to the
//  parent's old tip, run GRAFRebase onto the parent's new tip, capture
//  new tip into recs[].  Returns OK on success or a GRAFCNFL/error.
static ok64 post_cascade_one(cascade_ctx *cc, u8cs branch,
                             sha1cp parent_old_tip,
                             sha1cp parent_new_tip) {
    sane(cc);
    if (cc->n >= CASCADE_MAX) return SNIFFFAIL;

    sha1 child_tip = {};
    ok64 cr = POSTResolveBranchTip(&child_tip, branch);
    if (cr == REFSNONE) return OK;     //  branch has no REFS tip — skip
    if (cr != OK) return cr;

    //  Flat single-shard store: every branch's objects live in the one
    //  `<root>/.be/<project>/` shard, and the cascade shares a single
    //  open pack (`cc->p`) for the whole walk.  Objects emitted by the
    //  parent's same-branch rebase were flushed into a now-closed,
    //  registered pack; objects emitted by earlier cascade steps live
    //  in `cc->p` (visible to KEEPGetExact while it stays open).  So
    //  there is NO per-branch keeper/graf switch and NO pack close/
    //  reopen here — that dance (needed by the old per-branch layout)
    //  would drop visibility of the just-rebased parent tip and break
    //  `KEEPGetExact` mid-cascade.  GRAFLca/GRAFRebase below resolve all
    //  ancestry against the single shard.

    //  Old fork point = LCA(parent_old_tip, child_tip).
    sha1 fork_old = {};
    (void)GRAFLca(&fork_old, parent_old_tip, &child_tip);

    //  If the child's tip is already an ancestor of parent_new_tip,
    //  there is nothing to replay.  Detect: LCA(child_tip,
    //  parent_new_tip) == child_tip.
    {
        sha1 lca2 = {};
        (void)GRAFLca(&lca2, &child_tip, parent_new_tip);
        if (sha1Eq(&lca2, &child_tip)) {
            //  Child is already on new spine — point its REFS at the
            //  matching commit (child_tip itself).  No rebase needed.
            cascade_rec *r = &cc->recs[cc->n++];
            if (PATHu8bAren(cc->arena, r->branch, branch) != OK) {
                cc->n--;
                return SNIFFFAIL;
            }
            r->old_tip = child_tip;
            r->new_tip = child_tip;
            return OK;
        }
    }

    post_rebase_ctx rctx = {.p = cc->p};
    ok64 rb = GRAFRebase(&fork_old, parent_new_tip, &child_tip,
                         post_rebase_emit_cb, &rctx);
    if (rb != OK) return rb;

    cascade_rec *r = &cc->recs[cc->n++];
    if (PATHu8bAren(cc->arena, r->branch, branch) != OK) {
        cc->n--;
        return SNIFFFAIL;
    }
    r->old_tip = child_tip;
    r->new_tip = rctx.have_last_commit ? rctx.last_commit_sha
                                       : *parent_new_tip;
    return OK;
}

//  Collector for `post_cascade_collect_cb`: accumulates child-branch
//  absolute paths into a single arena, with one `u8cs` slice per name.
//  `parent` is the branch whose direct children we want (e.g. `L1`);
//  trunk-leaf is the empty slice.
typedef struct {
    u8cs  parent;            // branch whose direct children we collect
    u8cs *names;             // each: absolute child path (e.g. `L1/L2`)
    u32   cap;
    u32   n;
    u8bp  arena;
} pc_collect;

//  REFSEach callback: one entry per latest-per-key REFS row.  In the
//  flat single-shard store there are no per-branch object subdirs —
//  the branch hierarchy lives purely in REFS keys (`?L1`, `?L1/L2`).
//  A *direct child* of `parent` is a key `?<parent>/<seg>` with exactly
//  one extra path segment (no further `/`).  Trunk-leaf (empty parent)
//  matches any single-segment key `?<seg>`.  We record the child's
//  absolute branch path (`<parent>/<seg>` or `<seg>`).
static ok64 post_cascade_collect_cb(refcp r, void *ctx) {
    pc_collect *pc = (pc_collect *)ctx;
    if (pc->n >= pc->cap) return OK;
    a_dup(u8c, key, (u8c **)r->key);
    //  Keys are `?<branch>`; only local-branch keys (leading '?', no
    //  host) participate in the cascade.  Skip anything else.
    if (u8csEmpty(key) || *key[0] != '?') return OK;
    u8csUsed(key, 1);                          //  drop leading '?'
    //  `?` (bare) is trunk-leaf itself, never its own child.
    if (u8csEmpty(key)) return OK;
    //  Must sit under `parent`: consume the `<parent>/` prefix.  Build
    //  `<parent>/` once and prefix-check `key` against it.
    if (!u8csEmpty(pc->parent)) {
        a_pad(u8, pfxbuf, 256);
        u8bFeed(pfxbuf, pc->parent);
        u8bFeed1(pfxbuf, '/');
        a_dup(u8c, pfx, u8bData(pfxbuf));
        if (!u8csHasPrefix(key, pfx)) return OK;
        u8csUsed(key, u8csLen(pfx));           //  drop `<parent>/`
    }
    //  Remaining `key` is the child path relative to parent; a *direct*
    //  child has no further '/'.  Grandchildren are reached by recursion.
    if (u8csEmpty(key)) return OK;
    $for(u8c, p, key) {
        if (*p == '/') return OK;              //  not a direct child
    }
    //  Record the absolute child branch path.
    a_pad(u8, abs, 256);
    if (!u8csEmpty(pc->parent)) {
        u8bFeed(abs, pc->parent);
        u8bFeed1(abs, '/');
    }
    u8bFeed(abs, key);
    a_dup(u8c, abs_view, u8bData(abs));
    if (PATHu8bAren(pc->arena, pc->names[pc->n], abs_view) != OK) return OK;
    pc->n++;
    return OK;
}

//  Recursive walker.  For each REFS branch that is a direct child of
//  `branch`, stage its rebase, then recurse with the child as the new
//  parent.  `branch_old_tip` and `branch_new_tip` are the stage's
//  current parent before/after the rewrite.
static ok64 post_cascade_walk(cascade_ctx *cc, u8cs branch,
                              sha1cp branch_old_tip,
                              sha1cp branch_new_tip) {
    sane(cc);
    a_path(rdir);
    call(HOMEBranchDir, SNIFF.h, rdir, NULL);

    //  Snapshot direct-child branch paths via REFSEach; each `names[i]`
    //  is a slice into `names_arena`.  No children → cascade ends here.
    u8cs names[CASCADE_MAX] = {};
    a_carve(u8, names_arena, CASCADE_MAX * 256);
    pc_collect pc = {.names = names,
                     .cap = CASCADE_MAX, .arena = names_arena};
    u8csMv(pc.parent, branch);
    (void)REFSEach($path(rdir), post_cascade_collect_cb, &pc);
    u32 nfound = pc.n;

    for (u32 i = 0; i < nfound; i++) {
        a_dup(u8c, child_branch, names[i]);

        //  Capture the child's tip BEFORE the rebase via the global
        //  REFS lookup.  No tip → skip (stale/deleted row).
        sha1 child_old = {};
        ok64 cr = POSTResolveBranchTip(&child_old, child_branch);
        if (cr != OK) continue;     //  no row → skip

        //  Skip filter: cross-branch promote handles cur via auto-sync,
        //  so don't recurse into it from the cascade.
        if (!u8csEmpty(cc->skip) && u8csEq(child_branch, cc->skip)) continue;

        //  Stage rebase + record.
        ok64 ro = post_cascade_one(cc, child_branch,
                                   branch_old_tip, branch_new_tip);
        if (ro != OK) return ro;

        //  Recurse: this child's old/new tips drive the next level.
        sha1 child_new = cc->recs[cc->n - 1].new_tip;
        ok64 rr = post_cascade_walk(cc, child_branch, &child_old,
                                    &child_new);
        if (rr != OK) return rr;
    }
    return OK;
}

//  Persist staged cascade REFS writes.  Top-down (parent first ⇒ index
//  order).  CAS races during persist are surfaced but do not unwind
//  earlier successes (best-effort, documented).
static ok64 post_cascade_persist(cascade_ctx *cc) {
    sane(cc);
    a_path(keepdir);
    call(HOMEBranchDir, SNIFF.h, keepdir, NULL);
    for (u32 i = 0; i < cc->n; i++) {
        cascade_rec *r = &cc->recs[i];
        a_pad(u8, keybuf, 256);
        u8bFeed1(keybuf, '?');
        u8bFeed(keybuf, r->branch);
        a_dup(u8c, refkey, u8bData(keybuf));

        a_pad(u8, exp_hex, 40);
        a_rawc(esha, r->old_tip);
        HEXu8sFeedSome(exp_hex_idle, esha);
        a_pad(u8, new_hex, 40);
        a_rawc(nsha, r->new_tip);
        HEXu8sFeedSome(new_hex_idle, nsha);
        a_dup(u8c, expected, u8bDataC(exp_hex));
        a_dup(u8c, val,      u8bDataC(new_hex));

        ok64 cas = REFSCompareAndAppend($path(keepdir), refkey,
                                        expected, val);
        if (cas == REFSCAS) {
            fprintf(stderr,
                    "sniff: post: cascade REFS race on `?" U8SFMT
                    "` — earlier descendants may have advanced\n",
                    u8sFmt(r->branch));
        } else if (cas != OK) {
            return cas;
        }
    }
    return OK;
}

// --- Cross-branch promote dispatcher (Stage 2d) ---
//
//  `be post ?<X>` (no -m): the URI names a different branch and the
//  user wants cur's stack moved over.  Four shapes per VERBS.md §POST:
//
//    (a) ?..             upstream   target == dirname(cur)
//    (b) ?./fix          child      target == cur + '/' + name (existing)
//    (c) ?./newleaf      missing    target under cur, no REFS row → create
//    (d) ?<absolute>     peer       any other existing branch
//
//  Operand mapping (see also docstring on POSTPromote):
//
//    (a) base_old=LCA(parent.tip, cur.tip), base_new=parent.tip,
//        child_tip=cur.tip; cur auto-syncs.
//    (b) base_old=LCA(cur.tip, fix.tip),    base_new=cur.tip,
//        child_tip=fix.tip; cur unchanged.
//    (c) KEEPCreateBranch + REFS row at cur.tip; no rebase.
//    (d) base_old=LCA(target.tip, cur.tip), base_new=target.tip,
//        child_tip=cur.tip; cur auto-syncs iff target == dirname(cur).
//
//  Cur auto-sync is mechanically a second REFSCompareAndAppend after
//  the target write succeeds.  A CAS race on cur after target advanced
//  leaves cur stale (user can `be get ?..` to resync) — best-effort
//  MWP behaviour, documented inline below.

//  YES iff `s` starts with `prefix`.
static b8 post_starts_with(u8cs s, u8cs prefix) {
    return u8csHasPrefix(s, prefix);
}

//  dirname of an absolute branch path: drop trailing `/segment`.
//  Empty input (= trunk) yields empty.  Output is a slice into input.
static void post_dirname(u8cs out, u8cs abs_branch) {
    out[0] = abs_branch[0];
    out[1] = abs_branch[0];
    if (u8csEmpty(abs_branch)) return;
    u8cp last_slash = NULL;
    $for(u8c, p, abs_branch) {
        if (*p == '/') last_slash = p;
    }
    if (last_slash != NULL) out[1] = last_slash;
}

//  basename of an absolute branch path: bytes after the last '/'.
//  Empty input yields empty; no '/' yields the whole input.  Output
//  is a slice into input.
static void post_basename(u8cs out, u8cs abs_branch) {
    u8csMv(out, abs_branch);
    if (u8csEmpty(abs_branch)) return;
    u8cp last_slash = NULL;
    $for(u8c, p, abs_branch) {
        if (*p == '/') last_slash = p;
    }
    if (last_slash != NULL) out[0] = last_slash + 1;
}

ok64 POSTFpChainTo(sha1cp from, sha1cp stop,
                   sha1 *out, u32 cap, u32 *nout, b8 *reached_stop) {
    sane(from && out && nout && reached_stop);
    *nout = 0;
    *reached_stop = NO;
    if (cap == 0) done;

    a_carve(u8, cbuf, 1UL << 16);
    sha1 cur = *from;
    while (*nout < cap) {
        if (stop != NULL && sha1Eq(&cur, stop)) {
            *reached_stop = YES;
            break;
        }
        out[(*nout)++] = cur;
        u8bReset(cbuf);
        u8 ct = 0;
        if (KEEPGetExact(&cur, cbuf, &ct) != OK ||
            ct != DOG_OBJ_COMMIT) break;
        a_dup(u8c, body, u8bData(cbuf));
        u8cs field = {}, value = {};
        b8 found_par = NO;
        while (GITu8sDrainCommit(body, field, value) == OK) {
            if ($empty(field)) break;
            a_cstr(par_kw, "parent");
            if (u8csEq(field, par_kw) && u8csLen(value) >= 40) {
                a_head(u8c, hx, value, 40);
                a_raw(bn, cur);
                (void)HEXu8sDrainSome(bn, hx);
                found_par = YES;
                break;
            }
        }
        if (!found_par) break;
    }
    done;
}

ok64 POSTPromote(u8cs target_branch, b8 allow_create, u8cs verb_tag) {
    sane($ok(target_branch));
    keeper *k = &KEEP;
    //  Default verb tag is "post"; callers that originate from a PUT
    //  pass "put" so user-facing log lines name the verb the user typed.
    a_cstr(default_tag, "post");
    u8cs vtag = {};
    if (u8csEmpty(verb_tag)) u8csMv(vtag, default_tag);
    else                      u8csMv(vtag, verb_tag);
    //  Repo root reachable from SNIFF.h->root.  Use `a_dup` for a
    //  local, consumable copy where the body needs slice ops.
    a_dup(u8c, reporoot, post_reporoot());

    //  --- 1. Resolve cur (baseline branch + cur tip) ---
    a_pad(u8, cur_buf, 256);
    sha1  cur_tip       = {};
    b8    has_cur_tip   = NO;
    {
        ron60 ts = 0, verb = 0;
        uri u = {};
        ok64 br = SNIFFAtCurTip(&ts, &verb, &u);
        if (br == OK) {
            u8cs branch = {};
            DOGQueryBranchOnly(u.query, branch);
            if (!u8csEmpty(branch)) u8bFeed(cur_buf, branch);
            //  Cur tip from row's #fragment / first SHA in query.
            sha1hex hex = {};
            if (SNIFFAtQueryFirstSha(&u, &hex) == OK &&
                sha1FromSha1hex(&cur_tip, &hex) == OK)
                has_cur_tip = YES;
        }
    }
    a_dup(u8c, cur_branch, u8bData(cur_buf));

    if (!has_cur_tip) {
        fprintf(stderr,
                "sniff: " U8SFMT ": no cur tip — cannot promote\n",
                u8sFmt(vtag));
        return SNIFFFAIL;
    }

    //  --- 1b. Trailing-slash arm: `?<absolute>/` reuses cur's basename.
    //  Spec: `be post ?feat/` from cur on `?fix1` rewrites the target
    //  to `?feat/fix1`.  When cur is trunk (empty basename) or target
    //  is exactly "/", refuse: there's no name to copy.  After this,
    //  target_branch always names a leaf (no trailing slash).
    a_pad(u8, target_buf, 260);
    {
        b8 had_slash = NO;
        u8cs t_in = {target_branch[0], target_branch[1]};
        if (!$empty(t_in) && *u8csLast(t_in) == '/') {
            had_slash = YES;
            u8csShed1(t_in);   //  drop trailing '/'
        }
        if (had_slash) {
            u8cs base_in = {};
            post_basename(base_in, cur_branch);
            if ($empty(base_in)) {
                fprintf(stderr,
                        "sniff: " U8SFMT ": trailing-slash target needs a "
                        "non-empty cur basename\n",
                        u8sFmt(vtag));
                return SNIFFFAIL;
            }
            //  Rebuild target as `<stripped>/<basename(cur)>`.
            if (!$empty(t_in)) {
                u8bFeed(target_buf, t_in);
                u8bFeed1(target_buf, '/');
            }
            u8bFeed(target_buf, base_in);
            //  Rebind target_branch to the rewritten path.
            target_branch[0] = u8bDataHead(target_buf);
            target_branch[1] = u8bIdleHead(target_buf);
        }
    }

    //  --- 2. Same-branch guard: caller should have routed elsewhere. ---
    if (u8csEq(target_branch, cur_branch)) {
        //  Target == cur: not a promote.  The caller (label-only
        //  legacy path) will fall through to its own PUTSetLabel.
        return POSTNONE;
    }

    //  --- 3. Resolve target tip (may be missing → CREATE_ON_MISS). ---
    sha1 target_tip      = {};
    b8   target_exists   = NO;
    {
        ok64 tr = POSTResolveBranchTip(&target_tip, target_branch);
        if (tr == OK) target_exists = YES;
        else if (tr != REFSNONE) {
            //  REFSBAD or other read error — surface.
            return tr;
        }
    }

    //  Cross-branch promote: load the target branch's packs so the
    //  subsequent graf walks, KEEPGet calls, and POSTCommit pack
    //  write all see (and land in) the right shard.  No-op when the
    //  target dir doesn't exist on disk yet (CREATE_ON_MISS arm
    //  mkdir's it below before any keeper write).
    if (target_exists) {
        (void)SNIFFMaybeSwitchGraf(target_branch);
        (void)SNIFFMaybeSwitchKeeper(target_branch);
    }

    //  --- 4. Classify shape. ---
    //  is_child:  target startswith cur+'/'  (or cur empty + target nonempty)
    //  is_parent: target == dirname(cur)
    a_pad(u8, cur_with_slash, 260);
    if (!u8csEmpty(cur_branch)) {
        u8bFeed(cur_with_slash, cur_branch);
        u8bFeed1(cur_with_slash, '/');
    }
    a_dup(u8c, cur_slash, u8bData(cur_with_slash));
    b8 is_child = u8csEmpty(cur_branch)
                    ? !u8csEmpty(target_branch)
                    : post_starts_with(target_branch, cur_slash);
    u8cs cur_dir = {};
    post_dirname(cur_dir, cur_branch);
    b8 is_parent = NO;
    if (!u8csEmpty(cur_branch) && u8csEq(cur_dir, target_branch)) {
        is_parent = YES;
    }

    //  --- 5. CREATE_ON_MISS arm: ?./newleaf or ?<absolute>/<newleaf>. ---
    //  Spec: when the target doesn't exist, two shapes are accepted:
    //    (a) `?./X` — `is_child` (target under cur).  The new leaf
    //        sits at cur.tip; cur is unchanged.
    //    (b) `?<absolute>/<newleaf>` — `dirname(target)` is an existing
    //        branch.  Create the leaf, then replay cur's stack onto
    //        `dirname(target).tip` so the new leaf carries cur's
    //        commits on top of the absolute parent's tip.  Cur stays
    //        put.  Trailing-slash form (`?feat/`) was rewritten to
    //        `?feat/<basename(cur)>` in step 1b above and lands here.
    //
    //  Other create-on-miss shapes (`?../sib`, etc.) still fall back
    //  to the legacy PUTSetLabel via POSTNONE.
    //  Spec: POST never creates branches.  Branch creation lives in
    //  PUT (`be put ?<branch>`) — POST refuses unresolved refs.  The
    //  PUT-side wrapper (`PUTCreateBranch`) calls us with
    //  allow_create=YES to reuse the create-on-miss arm below.
    if (!target_exists && !allow_create) {
        fprintf(stderr,
                "sniff: " U8SFMT ": ?" U8SFMT " does not exist — "
                "`be put ?<branch>` first\n",
                u8sFmt(vtag), u8sFmt(target_branch));
        return POSTNONE;
    }

    sha1 absolute_parent_tip = {};
    b8   create_under_absolute = NO;
    if (!target_exists && !is_child) {
        //  Try arm (b): dirname(target) is a real branch.  Empty
        //  dirname means trunk (the root) — `?sib` / `?../sib` from
        //  a child both land here, with trunk's tip as the absolute
        //  parent.
        u8cs t_dir = {};
        post_dirname(t_dir, target_branch);
        ok64 dr = POSTResolveBranchTip(&absolute_parent_tip, t_dir);
        if (dr == OK) create_under_absolute = YES;
        else if (dr != REFSNONE) return dr;
        if (!create_under_absolute) {
            //  Parent branch missing — surface as POSTNONE.
            return POSTNONE;
        }
    }
    if (!target_exists) {
        //  Materialise the per-branch shard (idempotent on KEEPDUP).
        ok64 ko = KEEPCreateBranch(k->h, target_branch);
        if (ko != OK && ko != KEEPDUP && ko != KEEPTRUNK) return ko;

        //  Compute the new leaf's tip.  Default = cur_tip (arm `?./X`).
        //  For `?<absolute>/<newleaf>` (and trailing-slash rewrite), we
        //  rebase cur's stack onto absolute_parent_tip first and use
        //  the rebased tip.
        sha1 leaf_tip = cur_tip;
        if (create_under_absolute) {
            sha1 lca = {};
            (void)GRAFLca(&lca, &cur_tip, &absolute_parent_tip);
            if (sha1Eq(&lca, &cur_tip)) {
                //  Cur already on absolute_parent_tip's spine — leaf
                //  lands at absolute_parent_tip.
                leaf_tip = absolute_parent_tip;
            } else if (sha1Eq(&lca, &absolute_parent_tip)) {
                //  Cur is downstream of absolute parent — fast-forward
                //  case: leaf = cur_tip (no replay needed).
                leaf_tip = cur_tip;
            } else {
                //  Replay cur's stack onto absolute_parent_tip.
                keep_pack pp = {};
                call(KEEPPackOpen, &pp);
                pp.strict_order = NO;
                post_rebase_ctx rctx = {.p = &pp};
                ok64 rb = GRAFRebase(&lca, &absolute_parent_tip,
                                     &cur_tip, post_rebase_emit_cb,
                                     &rctx);
                ok64 cl = KEEPPackClose(&pp);
                if (rb != OK) {
                    fprintf(stderr,
                            "sniff: " U8SFMT ": leaf-create rebase aborted "
                            "(%s)\n",
                            u8sFmt(vtag),
                            rb == GRAFCNFL ? "merge conflict" : "error");
                    return rb;
                }
                if (cl != OK) return cl;
                leaf_tip = rctx.have_last_commit
                            ? rctx.last_commit_sha
                            : absolute_parent_tip;
            }
        }

        //  REFS row at leaf_tip with empty `expected_old`.
        a_path(keepdir);
        call(HOMEBranchDir, k->h, keepdir, NULL);
        a_pad(u8, refkey_buf, 260);
        u8bFeed1(refkey_buf, '?');
        u8bFeed(refkey_buf, target_branch);
        a_dup(u8c, refkey, u8bData(refkey_buf));

        a_pad(u8, val_hex, 40);
        a_rawc(vsha, leaf_tip);
        HEXu8sFeedSome(val_hex_idle, vsha);
        a_dup(u8c, val, u8bDataC(val_hex));
        a_cstr(empty_s, "");
        u8cs expected = {empty_s[0], empty_s[1]};

        ok64 cr = REFSCompareAndAppend($path(keepdir), refkey,
                                       expected, val);
        if (cr == REFSCAS) {
            //  Lost the race: someone else created the branch.  Retry
            //  as a PROMOTE on the now-existing branch.
            ok64 tr = POSTResolveBranchTip(&target_tip, target_branch);
            if (tr != OK) return cr;
            target_exists = YES;
            //  Fall through to PROMOTE arm below.
        } else if (cr != OK) {
            return cr;
        } else {
            //  Created.  Cur unchanged.  Done.
            fprintf(stderr,
                    "sniff: " U8SFMT ": created ?" U8SFMT " at " U8SFMT "\n",
                    u8sFmt(vtag),
                    u8sFmt(target_branch), u8sFmt(u8bDataC(val_hex)));
            return OK;
        }
    }

    //  --- 6. PROMOTE arm: target_exists is now true. ---
    //  Operand assignment per shape:
    sha1 base_old = {}, base_new = {}, child_tip = {};
    b8   advance_target_branch = YES;        //  always YES in promote
    b8   auto_sync_cur = NO;
    if (is_child) {
        //  ?./fix: replay fix's stack onto cur.tip.
        (void)GRAFLca(&base_old, &cur_tip, &target_tip);
        base_new  = cur_tip;
        child_tip = target_tip;
        auto_sync_cur = NO;
    } else {
        //  ?.. (parent) or ?<absolute> (peer/upstream): FF target to
        //  cur.tip when cur is a descendant of target.tip; refuse
        //  otherwise.  Detect via a keeper-side first-parent walk —
        //  graf's GRAFLca depends on the DAG being indexed for the
        //  cross-shard parent chain, which isn't guaranteed for
        //  freshly-fetched history.  Walking commit bodies directly
        //  is slower per step but always correct.
        #define POST_PARENT_MAX 8192
        sha1 cur = cur_tip;
        b8 ff = NO;
        a_carve(u8, cbuf, 1UL << 16);
        for (u32 hop = 0; hop < POST_PARENT_MAX; hop++) {
            if (sha1Eq(&cur, &target_tip)) { ff = YES; break; }
            u8bReset(cbuf);
            u8 ct = 0;
            if (KEEPGetExact(&cur, cbuf, &ct) != OK ||
                ct != DOG_OBJ_COMMIT) break;
            a_dup(u8c, body, u8bData(cbuf));
            u8cs field = {}, value = {};
            b8 stepped = NO;
            while (GITu8sDrainCommit(body, field, value) == OK) {
                if ($empty(field)) break;
                a_cstr(par_kw, "parent");
                if (u8csEq(field, par_kw) && u8csLen(value) >= 40) {
                    a_head(u8c, hx, value, 40);
                    a_raw(bn, cur);
                    if (HEXu8sDrainSome(bn, hx) == OK) stepped = YES;
                    break;
                }
            }
            if (!stepped) break;
        }
        if (ff) base_old = target_tip;
        else    sha1Zero(&base_old);  //  signal non-FF below
        base_new  = target_tip;
        child_tip = cur_tip;
        //  Spec (VERBS.md §POST): the named target advances; cur is
        //  never auto-modified.  User runs `be get ?<target>` if they
        //  want the wt to follow.
        auto_sync_cur = NO;
    }

    //  Already in sync? base_old == child_tip means there are no
    //  commits to replay (child is an ancestor of base_new).
    b8 nothing_to_replay = sha1Eq(&base_old, &child_tip);
    //  Also, if target already contains child_tip (ff in the other
    //  direction), nothing to do.
    if (nothing_to_replay) {
        sha1 lca2 = {};
        (void)GRAFLca(&lca2, &child_tip, &base_new);
        if (sha1Eq(&lca2, &child_tip)) {
            //  child_tip is an ancestor of base_new — no advance.
            //  But if auto_sync_cur is set, we still want cur to track
            //  base_new (e.g. `?..` after parent has been advanced
            //  already with cur's commits — sync cur to parent.tip).
            if (auto_sync_cur) {
                a_path(keepdir);
                call(HOMEBranchDir, k->h, keepdir, NULL);
                a_pad(u8, ckbuf, 260);
                u8bFeed1(ckbuf, '?');
                if (!u8csEmpty(cur_branch)) u8bFeed(ckbuf, cur_branch);
                a_dup(u8c, crefkey, u8bData(ckbuf));

                a_pad(u8, exp_hex, 40);
                a_rawc(esha, cur_tip);
                HEXu8sFeedSome(exp_hex_idle, esha);
                a_pad(u8, new_hex, 40);
                a_rawc(nsha, base_new);
                HEXu8sFeedSome(new_hex_idle, nsha);
                a_dup(u8c, expected, u8bDataC(exp_hex));
                a_dup(u8c, val,      u8bDataC(new_hex));
                ok64 cas = REFSCompareAndAppend($path(keepdir), crefkey,
                                                expected, val);
                if (cas == REFSCAS) {
                    fprintf(stderr,
                            "sniff: " U8SFMT ": cur auto-sync raced on "
                            "?" U8SFMT " — run `be get ?..` to refresh\n",
                            u8sFmt(vtag), u8sFmt(cur_branch));
                } else if (cas != OK) return cas;
            }
            fprintf(stderr,
                    "sniff: " U8SFMT ": nothing to promote (?" U8SFMT " already "
                    "contains cur)\n",
                    u8sFmt(vtag), u8sFmt(target_branch));
            return OK;
        }
    }

    //  --- 7. Fast-forward early-out.  If base_old == base_new, target
    //  hasn't moved since cur was forked: the "rebase" reduces to
    //  "advance target REFS to child_tip" with no replay needed.
    //  Skipping GRAFRebase here avoids spinning up a pack for the
    //  trivial case (and dodges any rebase-loop edge cases for it).
    sha1 target_new_tip = {};
    b8   stack_was_rewritten = NO;
    if (sha1Eq(&base_old, &base_new)) {
        target_new_tip = child_tip;
        // Flat store: child_tip's objects are already in the shared pool; FF is a pure REFS advance below.
        stack_was_rewritten = NO;
    } else {
        //  POST is commit-or-FF, never rebase (per VERBS.md).  When
        //  cur is not a descendant of target.tip, refuse with
        //  POSTNOFF — user runs `be patch ?target#` + `be post` per
        //  commit to rebase explicitly.  (Old `GRAFRebase` path
        //  removed: rebase semantics belong in PATCH; see
        //  VERBS.md §POST and the cheat sheet for `git rebase`.)
        fprintf(stderr,
                "sniff: " U8SFMT ": ?" U8SFMT " — not a fast-forward (cur is "
                "not a descendant of target.tip); use `be patch ?" U8SFMT
                "#` + `be post` to rebase\n",
                u8sFmt(vtag), u8sFmt(target_branch), u8sFmt(target_branch));
        return POSTNOFF;
    }

    //  --- 8. Cascade walk on the *target* side. ---
    //  After target's stack got rewritten (if anything was replayed),
    //  every descendant of target needs its fork point bumped.  The
    //  cascade walker is generalised — it takes a starting branch and
    //  the (old, new) tip pair; we just hand it the target's view.
    cascade_ctx casc = {};
    if (stack_was_rewritten) {
        //  Cascade record arena — BASS-carved, lives to function end
        //  (consumed by post_cascade_persist after the REFS update).
        a_carve(u8, casc_arena, CASCADE_MAX * 256);
        ((u8 **)casc.arena)[0] = casc_arena[0];
        ((u8 **)casc.arena)[1] = casc_arena[1];
        ((u8 **)casc.arena)[2] = casc_arena[2];
        ((u8 **)casc.arena)[3] = casc_arena[3];
        keep_pack p3 = {};
        call(KEEPPackOpen, &p3);
        p3.strict_order = NO;
        casc.p = &p3;
        //  Skip cur during cascade: auto-sync handles it directly.
        u8csMv(casc.skip, cur_branch);
        ok64 cw = post_cascade_walk(&casc, target_branch,
                                    &target_tip, &target_new_tip);
        ok64 cl3 = KEEPPackClose(&p3);
        if (cw != OK) {
            fprintf(stderr,
                    "sniff: " U8SFMT ": cascade aborted (%s)\n",
                    u8sFmt(vtag),
                    cw == GRAFCNFL ? "merge conflict in descendant"
                                   : "error");
            return cw;
        }
        if (cl3 != OK) return cl3;
    }

    //  --- 9. Advance target's REFS row via CAS on target_tip. ---
    {
        a_path(keepdir);
        call(HOMEBranchDir, k->h, keepdir, NULL);
        a_pad(u8, refkey_buf, 260);
        u8bFeed1(refkey_buf, '?');
        if (!u8csEmpty(target_branch)) u8bFeed(refkey_buf, target_branch);
        a_dup(u8c, refkey, u8bData(refkey_buf));

        a_pad(u8, exp_hex, 40);
        a_rawc(esha, target_tip);
        HEXu8sFeedSome(exp_hex_idle, esha);
        a_pad(u8, new_hex, 40);
        a_rawc(nsha, target_new_tip);
        HEXu8sFeedSome(new_hex_idle, nsha);
        a_dup(u8c, expected, u8bDataC(exp_hex));
        a_dup(u8c, val,      u8bDataC(new_hex));

        ok64 cas = REFSCompareAndAppend($path(keepdir), refkey,
                                        expected, val);
        if (cas == REFSCAS) {
            fprintf(stderr,
                    "sniff: " U8SFMT ": REFS for `?" U8SFMT "` advanced "
                    "concurrently — retry\n",
                    u8sFmt(vtag), u8sFmt(target_branch));
            return REFSCAS;
        }
        if (cas != OK) return cas;
    }
    (void)advance_target_branch;

    //  --- 10. Persist any cascade descendants (best-effort). ---
    if (casc.n > 0) (void)post_cascade_persist(&casc);

    //  --- 11. Cur auto-sync (?.. and ?<absolute> when target IS cur's
    //  tree-parent).  Race story: if this CAS loses, target already
    //  advanced and cur is stale — user can `be get ?..` to resync.
    //  Documented MWP best-effort behaviour. ---
    if (auto_sync_cur) {
        a_path(keepdir);
        call(HOMEBranchDir, k->h, keepdir, NULL);
        a_pad(u8, ckbuf, 260);
        u8bFeed1(ckbuf, '?');
        if (!u8csEmpty(cur_branch)) u8bFeed(ckbuf, cur_branch);
        a_dup(u8c, crefkey, u8bData(ckbuf));

        a_pad(u8, exp_hex, 40);
        a_rawc(esha, cur_tip);
        HEXu8sFeedSome(exp_hex_idle, esha);
        a_pad(u8, new_hex, 40);
        a_rawc(nsha, target_new_tip);
        HEXu8sFeedSome(new_hex_idle, nsha);
        a_dup(u8c, expected, u8bDataC(exp_hex));
        a_dup(u8c, val,      u8bDataC(new_hex));
        ok64 cas = REFSCompareAndAppend($path(keepdir), crefkey,
                                        expected, val);
        if (cas == REFSCAS) {
            fprintf(stderr,
                    "sniff: " U8SFMT ": cur auto-sync raced on `?" U8SFMT "` "
                    "— run `be get ?..` to refresh\n",
                    u8sFmt(vtag), u8sFmt(cur_branch));
            //  Don't surface — target already advanced; cur staleness
            //  is recoverable.
        } else if (cas != OK) return cas;
        else if (stack_was_rewritten) {
            //  Cur's REFS now points at target_new_tip but the wt
            //  still holds the pre-rebase tree.  Materialize the new
            //  tree on disk so the caller doesn't need an explicit
            //  follow-up `be get`.  Source URI = `?<cur_branch>`.
            a_pad(u8, hex_buf, 40);
            a_rawc(nsha2, target_new_tip);
            HEXu8sFeedSome(hex_buf_idle, nsha2);
            a_dup(u8c, hex_cs, u8bDataC(hex_buf));
            a_pad(u8, src_buf, 260);
            u8bFeed1(src_buf, '?');
            if (!u8csEmpty(cur_branch)) u8bFeed(src_buf, cur_branch);
            a_dup(u8c, src_cs, u8bDataC(src_buf));
            (void)GETCheckout(reporoot, hex_cs, src_cs);
        }
    }

    //  Done.  Pretty-print the resulting tip for the user.
    {
        a_pad(u8, hex_out, 40);
        a_rawc(osha, target_new_tip);
        HEXu8sFeedSome(hex_out_idle, osha);
        fprintf(stderr,
                "sniff: " U8SFMT ": ?" U8SFMT " -> " U8SFMT "\n",
                u8sFmt(vtag), u8sFmt(target_branch), u8sFmt(u8bDataC(hex_out)));
    }
    return OK;
}

// --- Public API ---

static ok64 post_print_status_inner(post_ctx *c) {
    sane(c);
    sha1 base_tree_sha = {};
    b8   have_base = NO;
    call(post_scan_changeset, c, &base_tree_sha, &have_base);

    //  Walk decisions, print one line per changed path.
    post_mad_ctx mad = {.fd = STDOUT_FILENO, .changed = 0};
    post_walk_decisions(c, POST_VM_UNLINK | POST_VM_ADD,
                        post_drain_mad_cb, &mad);
    fflush(stdout);
    fprintf(stderr, "sniff: %u change(s)\n", mad.changed);
    done;
}

ok64 POSTPrintStatus(void) {
    sane(1);
    post_ctx ctx = {};
    a_carve(u8, decisions, POST_TREE_ULOG_MAX);
    call(post_ctx_init, &ctx, decisions);
    try(post_print_status_inner, &ctx);
    done;
}

ok64 POSTPatchDefaults(u8b msg_buf,  u8cs *msg_out,
                       u8b auth_buf, u8cs *auth_out,
                       u32 *n_out) {
    sane(Bok(msg_buf) && Bok(auth_buf) && msg_out && auth_out && n_out);
    *n_out = 0;

    //  New msg-resolution per VERBS.md §POST:
    //    1. POST's own #frag wins (handled by caller before us).
    //    2. Else if exactly one in-scope patch row applied a commit
    //       AND it carries a usable msg — use it.  Usable msgs:
    //         MERGE   → row's fragment (user-supplied).
    //         CHERRY  → picked commit's subject (looked up here).
    //         REBASE1 → replayed commit's subject (looked up here).
    //         SQUASH  → not usable (multiple commits collapsed).
    //    3. Else (0 or >1 usable msgs) → return ULOGNONE; caller
    //       refuses with POSTNOMSG.
    sniff_pe pent[64];
    u32 n_pent = 0;
    (void)SNIFFAtPatchEntries(pent, 64, &n_pent);
    if (n_pent == 0) return ULOGNONE;
    *n_out = n_pent;

    //  Count usable-msg entries; remember the last one's index for
    //  the "exactly one" path.
    u32 usable = 0, idx = 0;
    for (u32 i = 0; i < n_pent; i++) {
        u8 sh = pent[i].shape;
        if (sh == 1 /* SQUASH */) continue;
        usable++;
        idx = i;
    }
    if (usable == 0) return ULOGNONE;
    if (usable > 1) {
        //  Ambiguous: surface every candidate so the user can pick
        //  one to retype as `#frag` on retry.  Switch keeper to the
        //  picked branch (or cur, for non-located rows) before
        //  KEEPGetExact so subject reads succeed even when the
        //  picked sha lives in a sibling shard.
        fprintf(stderr,
                "sniff: post: multiple eligible messages — re-run "
                "with explicit `#<subject>` to pick one:\n");
        i64 now = 0;
        {
            struct timespec tv = {};
            if (clock_gettime(CLOCK_REALTIME, &tv) == 0) now = tv.tv_sec;
        }
        a_pad(u8, saved, 256);
        u8bFeed(saved, u8bDataC(KEEP.h->cur_branch));
        if (u8bDataLen(saved) > 0 && *u8bLast(saved) == '/')
            u8bShed1(saved);
        a_carve(u8, cbuf, 1UL << 16);
        for (u32 i = 0; i < n_pent; i++) {
            if (pent[i].shape == 1 /* SQUASH */) continue;
            //  Switch to the row's locator branch (if any) so a
            //  rebase-one against `?feat#` can read feat's pack.
            //  No locator → stay on cur.
            b8 switched = NO;
            if (!u8csEmpty(pent[i].locator)) {
                if (SNIFFMaybeSwitchKeeper(pent[i].locator) == OK)
                    switched = YES;
            }
            u8bReset(cbuf);
            u8 ct = 0;
            ok64 ko = KEEPGetExact(&pent[i].sha, cbuf, &ct);
            //  Fallback: emit a minimal "sha + locator" hint when the
            //  body isn't reachable (sibling shard not on the open
            //  chain).  User can still match by sha.
            a_pad(u8, date, 8);
            u8cs subject = {};
            if (ko == OK && ct == DOG_OBJ_COMMIT) {
                git_commit gc = {};
                GITu8sParseCommit(u8bDataC(cbuf), &gc);
                u8csMv(subject, gc.subject);
                i64 secs = 0;
                struct tm t = {};
                if (RONToTime(gc.author_ts, &t, NULL) == OK) {
                    t.tm_isdst = -1;
                    time_t s = mktime(&t);
                    if (s != (time_t)-1) secs = (i64)s;
                }
                if (secs > 0) (void)DOGutf8sFeedDate(date_idle, secs, now);
            }
            if (u8bDataLen(date) == 0) {
                a_cstr(sp7, "       ");
                (void)u8bFeed(date, sp7);
            }
            if (!u8csEmpty(subject)) {
                fprintf(stderr, "  %.*s\tpat\t#%.*s\n",
                        (int)u8bDataLen(date),
                        (char *)u8bDataHead(date),
                        (int)$len(subject), (char *)subject[0]);
            } else {
                //  Body unreachable — show sha (8 hex) + locator hint.
                sha1hex hx = {};
                sha1hexFromSha1(&hx, &pent[i].sha);
                if (!u8csEmpty(pent[i].locator)) {
                    fprintf(stderr,
                            "  %.*s\tpat\t#%.8s (in ?%.*s — body "
                            "not in cur's shard)\n",
                            (int)u8bDataLen(date),
                            (char *)u8bDataHead(date),
                            hx.data,
                            (int)$len(pent[i].locator),
                            (char *)pent[i].locator[0]);
                } else {
                    fprintf(stderr,
                            "  %.*s\tpat\t#%.8s (body not in cur's "
                            "shard)\n",
                            (int)u8bDataLen(date),
                            (char *)u8bDataHead(date),
                            hx.data);
                }
            }
            if (switched) {
                a_dup(u8c, sb, u8bData(saved));
                (void)KEEPSwitchBranch(KEEP.h, sb);
            }
        }
        return ULOGNONE;
    }

    sha1 pick = pent[idx].sha;
    u8   pshape = pent[idx].shape;

    //  Located cherry-pick rows (`?<branch>/<sha>`) carry the branch
    //  prefix in `pent[idx].locator`.  Switch keeper to that branch
    //  long enough to read the picked commit's body, then switch
    //  BACK so any subsequent POSTCommit pack write lands in cur's
    //  shard, not the locator's.
    a_pad(u8, saved_branch, 256);
    b8 switched = NO;
    if (!u8csEmpty(pent[idx].locator)) {
        u8bFeed(saved_branch, u8bDataC(KEEP.h->cur_branch));
        //  KEEPOpenBranch normalises with a trailing '/'; strip it
        //  so the round-trip via DPATHBranchNormFeed doesn't
        //  re-add another.  Empty saved_branch (= trunk) stays
        //  empty.
        if (u8bDataLen(saved_branch) > 0 &&
            *u8bLast(saved_branch) == '/')
            u8bShed1(saved_branch);
        //  Graf reads h->cur_branch as its "from" — order before keeper.
        (void)SNIFFMaybeSwitchGraf(pent[idx].locator);
        ok64 so = SNIFFMaybeSwitchKeeper(pent[idx].locator);
        switched = (so == OK);
    }

    a_carve(u8, cbuf, 1UL << 16);

    u8 ct = 0;
    ok64 ko = KEEPGetExact(&pick, cbuf, &ct);

    if (switched) {
        a_dup(u8c, sb, u8bData(saved_branch));
        //  Graf BEFORE keeper: graf reads h->cur_branch as its "from"
        //  for the LCA delta; keeper is the one that updates
        //  h->cur_branch via HOMESetCurBranch.  Reversed order would
        //  feed graf the post-switch value and collapse the delta.
        (void)GRAFSwitchBranch(GRAF.h, sb);
        (void)KEEPSwitchBranch(KEEP.h, sb);
    }
    if (ko != OK) return ko;
    if (ct != DOG_OBJ_COMMIT) fail(SNIFFFAIL);

    git_commit gc = {};
    GITu8sParseCommit(u8bDataC(cbuf), &gc);
    u8cs pick_subject   = {};
    u8cs pick_author_id = {};
    u8csMv(pick_subject,   gc.subject);
    u8csMv(pick_author_id, gc.author_id);

    //  MERGE shape: msg is the row's fragment (user-supplied),
    //  override the keeper-fetched subject.
    if (pshape == 3 /* MERGE */ && !u8csEmpty(pent[idx].msg)) {
        $mv(pick_subject, pent[idx].msg);
    }

    //  Et-al doesn't apply with the new "exactly one" rule.
    b8 et_al = NO;

    //  Compose author into auth_buf.  When et-al, inject " (et al)"
    //  before "<email>" — find the last '<' in the identity string.
    if (!et_al) {
        u8bFeed(auth_buf, pick_author_id);
    } else {
        u8c *email_lt = NULL;
        $for(u8c, p, pick_author_id) {
            if (*p == '<') email_lt = (u8c *)p;
        }
        if (email_lt == NULL) {
            //  Malformed identity (no '<') — append at end.
            u8bFeed(auth_buf, pick_author_id);
            a_cstr(et_al_suf, " (et al)");
            u8bFeed(auth_buf, et_al_suf);
        } else {
            u8cs name_part  = {pick_author_id[0], email_lt};
            u8cs email_part = {email_lt, pick_author_id[1]};
            while (!u8csEmpty(name_part) && *u8csLast(name_part) == ' ')
                u8csShed1(name_part);
            u8bFeed(auth_buf, name_part);
            a_cstr(et_al_mid, " (et al) ");
            u8bFeed(auth_buf, et_al_mid);
            u8bFeed(auth_buf, email_part);
        }
    }
    u8csMv(*auth_out, u8bDataC(auth_buf));

    //  Single-msg path under the new spec — no `(+N)` annotation
    //  (that was the old "absorb chain into one squash" behavior).
    u8bFeed(msg_buf, pick_subject);
    u8csMv(*msg_out, u8bDataC(msg_buf));

    done;
}

ok64 POSTCommit(u8cs target_branch,
                u8cs message, u8cs author,
                cli const *inv, sha1 *sha_out) {
    sane($ok(message) && $ok(author) && sha_out);
    //  DIS-009: refuse to commit from a detached wt (`?<sha>` cur-tip).
    //  A detached checkout has no branch to record the new commit
    //  against; committing would silently graft it onto trunk/empty
    //  branch.  Re-attach (`be get ?<branch>`) first.  Trunk-state
    //  `?#<sha>` is NOT detached and is allowed through (commits back
    //  to trunk).
    //  Detached wt (`?<sha>` cur-tip): refuse ONLY when no explicit
    //  branch target was given.  With a target (`be post ?/proj/branch`
    //  — e.g. a parent POST recursing into a beagle submodule), commit
    //  onto that branch, auto-creating it at the detached pin: the new
    //  commit becomes a real REFS tip, GC-reachable inside the sub's
    //  self-contained shard (a ref-less detached commit would be
    //  dropped at epoch recompaction — STORE.md §"recompacts in
    //  isolation").  The wt attaches to the target.  A bare detached
    //  `be post` (no target) still refuses — there is no branch to
    //  record the commit against.
    if (post_is_detached_wt() && u8csEmpty(target_branch)) {
        fprintf(stderr,
            "sniff: post: refusing on detached wt — re-attach to a "
            "branch first (be get ?<branch>)\n");
        return POSTDET;
    }
    b8 force = inv && CLIHas(inv, "--force");
    keeper *k = &KEEP;
    //  Repo root from the SNIFF singleton (see post_reporoot()).
    a_dup(u8c, reporoot, post_reporoot());

    //  1. Resolve baseline parent.  Single-parent invariant on the
    //     write path (see VERBS.md §POST):
    //       * no baseline row at all  → root commit (0 parents, OK).
    //       * baseline + parent sha   → normal commit.
    //       * baseline + no sha       → corrupt at-log; refuse.
    a_pad(u8, brbuf, 256);
    sha1  parent     = {};
    b8    has_parent = NO;
    b8    had_baseline = NO;
    ok64  br = post_collect_parents(brbuf, &parent, &has_parent,
                                    &had_baseline);
    if (br != OK) return br;
    if (had_baseline && !has_parent) {
        fprintf(stderr,
                "sniff: post: baseline at-log row has no parent SHA — "
                "refusing parentless commit (would orphan peer history "
                "on push)\n");
        return SNIFFFAIL;
    }

    //  Cross-branch override: when the caller passes a non-empty
    //  branch-shaped target (DOGRefIsBranch=YES — has '/', is `.`/`..`,
    //  or names an existing dir ref), the new commit lands on that
    //  branch and keeper/graf swap shards.  Tag-shaped targets (single
    //  segment, no slash) are pure labels per VERBS.md §POST: commit
    //  lands on cur (brbuf unchanged), then a REFS row points the tag
    //  at the new tip — no shard swap, no cur rewrite.  See
    //  `dog/DOG.h §DOGRefIsBranch`.
    //
    //  A trailing slash on the target (`?feat/`) is the "new branch"
    //  syntactic marker per VERBS.md §"Ref kinds": branch with no
    //  hierarchy.  The slash itself is stripped before storage — the
    //  canonical ref key is `?feat`, the slash was just the signal.
    b8 target_is_branch = NO;
    u8cs target_canon = {};
    u8csMv(target_canon, target_branch);
    if ($ok(target_canon) && !u8csEmpty(target_canon)) {
        target_is_branch = DOGRefIsBranch(target_canon);
        if (target_is_branch && *u8csLast(target_canon) == '/')
            u8csShed1(target_canon);
    }
    if (target_is_branch) {
        u8bReset(brbuf);
        u8bFeed(brbuf, target_canon);

        //  Re-target keeper to the cross-branch destination so the
        //  pack about to be written lands at `<root>/.be/<target>/
        //  NNNN.keeper` (per KEEP.h §"Branch-aware object store").
        //  No-op when there's no `<target>/` shard dir yet (the
        //  CREATE_ON_MISS arm via POSTPromote mkdir's it first).
        (void)SNIFFMaybeSwitchGraf(target_canon); (void)SNIFFMaybeSwitchKeeper(target_canon);
    }
    //  No baseline branch recovered AND no override → default to
    //  trunk (empty be-side query).  Locally trunk has no name; the
    //  wire layer aliases it to refs/heads/main.  brbuf left empty
    //  signals "trunk" to the ULOG/REFS writers below.

    //  --- ff-only pre-flight ----------------------------------------
    //  POST onto a branch with a recorded REFS tip is fast-forward
    //  only: the target's tip must be an ancestor of (or equal to)
    //  the wt's first parent.  Different = someone advanced the
    //  branch since our last sync, OR this is a cross-branch POST
    //  whose target is on an unrelated lineage; either way, refuse
    //  and let the user resolve manually (`be patch ?<branch>` for
    //  same-branch divergence, or `be delete ?<branch>` followed by
    //  recreate for cross-branch reset).
    //  Resolve target REFS tip up-front.  When present and != parent,
    //  the post-pack-feed REFS write below uses CAS on `expected_old =
    //  <tip>` so concurrent posters see REFSCAS.  Today this is a
    //  legacy ff-only pre-flight: we still bail with SNIFFNOFF on
    //  non-ff, the rebase-or-promote pathway is implemented in the
    //  caller (sniff post phase 2).  See VERBS.md §POST for the
    //  ff-or-rebase shape that replaces it.
    sha1 expected_tip_sha = {};
    b8   has_expected_tip = NO;
    b8   needs_rebase     = NO;   //  set when REFS tip diverges from cur's
                                  //  parent — replay the just-built commit
                                  //  onto REFS tip after the pack feed.
    if (had_baseline && has_parent) {
        a_path(keepdir);
        call(HOMEBranchDir, k->h, keepdir, NULL);
        a_pad(u8, refkey_buf, 128);
        u8bFeed1(refkey_buf, '?');
        a_dup(u8c, branch, u8bData(brbuf));
        if (!u8csEmpty(branch)) u8bFeed(refkey_buf, branch);
        a_dup(u8c, refkey_s, u8bData(refkey_buf));

        a_pad(u8, arena, 1024);
        uri resolved = {};
        ok64 ro = REFSResolve(&resolved, arena, $path(keepdir),
                              refkey_s);
        if (ro == OK && !$empty(resolved.query)) {
            a_dup(u8c, tip_hex, resolved.query);
            if (!u8csEmpty(tip_hex) && *tip_hex[0] == '?')
                u8csUsed(tip_hex, 1);
            if ($len(tip_hex) != 40) {
                fprintf(stderr,
                        "sniff: post: REFS row for `?" U8SFMT "` has "
                        "malformed tip (%zu bytes, want 40 hex)\n",
                        u8sFmt(branch), (size_t)$len(tip_hex));
                return SNIFFFAIL;
            }
            a_raw(bin, expected_tip_sha);
            a_dup(u8c, hx, tip_hex);
            ok64 ho = HEXu8sDrainSome(bin, hx);
            if (ho != OK) {
                fprintf(stderr,
                        "sniff: post: REFS row for `?" U8SFMT "` has "
                        "non-hex tip\n",
                        u8sFmt(branch));
                return SNIFFFAIL;
            }
            has_expected_tip = YES;

            //  ff iff tip is an ancestor of (or equal to) parent.
            b8 ff_ok = NO;
            if (sha1Eq(&parent, &expected_tip_sha)) {
                ff_ok = YES;
            } else {
                sha1 lca = {};
                (void)GRAFLca(&lca, &parent, &expected_tip_sha);
                if (sha1Eq(&lca, &expected_tip_sha)) ff_ok = YES;
            }
            if (!ff_ok) {
                //  Same-branch divergence: defer the SNIFFNOFF bail —
                //  the new commit will be rebased onto REFS tip after
                //  the pack feed (Stage 2 phase-2 promote).
                needs_rebase = YES;
            }
        }
    }
    //  --- end ff-only pre-flight ------------------------------------

    //  Steps 2..5 — the change-set scan — share their entire body
    //  with POSTPrintStatus's dry-run path.  See post_scan_changeset.
    post_ctx ctx = {};
    a_carve(u8, decisions, POST_TREE_ULOG_MAX);
    call(post_ctx_init, &ctx, decisions);

    sha1 base_tree_sha = {};
    b8   have_base = NO;
    call(post_scan_changeset, &ctx, &base_tree_sha, &have_base);

    //  5b. Unlink files marked for delete on disk.  Done BEFORE the
    //      pack feed so a follow-up `be post` doesn't pick them up via
    //      auto-stage; mtime-attribution fix for the BEhistory
    //      "deleted-file re-added" regression.
    post_walk_decisions(&ctx, POST_VM_UNLINK, post_drain_unlink_cb, NULL);

    //  6b. Single-parent invariant on the write path.  PATCH no longer
    //      records `&<theirs>` in baseline; the new commit takes the
    //      one parent recorded in `parent` (set by post_collect_parents).

    //  7. Build trees bottom-up over the dense lex-sorted recs the
    //     classification merge populated.
    sha1 root_tree = {};
    b8 have_root = NO;
    a_carve(u8, tree_bodies, 1UL << 20);
    u32 tree_count = 0;

    {
        //  post_build_tree walks the decisions ULOG directly: rows
        //  are lex-sorted by path, so subdir entries form contiguous
        //  byte sub-ranges sliced off via subslice recursion.
        a_dup(u8c, decisions_view, u8bData(ctx.decisions));
        u8cs no_prefix = {};
        call(post_build_tree, decisions_view, no_prefix,
                              &root_tree, tree_bodies, &tree_count);
        have_root = !sha1empty(&root_tree);
    }

    //  7b. Empty-commit refuse: if the new root tree matches the
    //      baseline's tree exactly, the wt has nothing to record.
    //      Refusing here keeps `.be/wtlog` and REFS clean — VERBS.md
    //      says "empty POSTs are refused."  Skip on a fresh repo
    //      (no baseline tree to compare against).
    if (had_baseline && have_root && have_base &&
        sha1Eq(&root_tree, &base_tree_sha)) {
        //  `-q` / `--quiet` (be's POST sub-recursion sets this for
        //  every sibling shard) suppresses the stderr note; the
        //  POSTNONE return is still used by the caller.
        if (!SNIFF.quiet)
            fprintf(stderr, "POSTNONE: no changes since base\n");
        return POSTNONE;
    }

    //  8. If the result has no files, fall back to the empty-tree sha.
    keep_pack p = {};
    call(KEEPPackOpen, &p);
    p.strict_order = NO;

    if (!have_root) {
        call(post_feed_empty_tree, k, &p, &root_tree);
    }

    //  9. Verify each parent commit exists locally; refuse otherwise.
    //     `parents[]` already holds the decoded sha1 bytes from the
    //     baseline row's ref-chunk scan; `post_parent_sha` re-runs the
    //     keeper lookup as a sanity check.
    if (has_parent) {
        a_pad(u8, hx_buf, 40);
        a_rawc(psha_in, parent);
        HEXu8sFeedSome(hx_buf_idle, psha_in);
        a_dup(u8c, ph, u8bDataC(hx_buf));
        sha1 ps = {};
        if (post_parent_sha(k, ph, &ps) != OK) {
            fprintf(stderr,
                    "sniff: post: parent commit " U8SFMT " not found in "
                    "keeper — refusing\n",
                    u8sFmt(ph));
            KEEPPackClose(&p);
            return SNIFFFAIL;
        }
        parent = ps;
    }

    //  10. Build commit body.  Single-parent invariant: at most one
    //      `parent <hex>\n` line.
    a_pad(u8, com, 4096);
    a_cstr(tree_label, "tree ");
    u8bFeed(com, tree_label);
    a_pad(u8, thex, 40);
    a_rawc(tsha, root_tree);
    HEXu8sFeedSome(thex_idle, tsha);
    u8bFeed(com, u8bDataC(thex));
    u8bFeed1(com, '\n');

    if (has_parent) {
        a_cstr(par_label, "parent ");
        u8bFeed(com, par_label);
        a_pad(u8, par_hex, 40);
        a_rawc(psha, parent);
        HEXu8sFeedSome(par_hex_idle, psha);
        u8bFeed(com, u8bDataC(par_hex));
        u8bFeed1(com, '\n');
    }

    //  Headers from in-scope patch rows (VERBS.md §POST "Parent /
    //  foster / picked assembly").  Classify each row by URI shape:
    //
    //    SQUASH   `?<sha>`         → foster <sha>\n   (after committer)
    //    REBASE1  `?<sha>#`        → foster <sha>\n   (after committer)
    //    MERGE    `?<sha>#<msg>`   → parent <sha>\n   (here, pre-author)
    //    CHERRY   `#<sha>`         → collected for `picked: <sha>`
    //                                 trailer appended to msg below.
    //
    //  Git's commit grammar is strict: `tree`, then ALL `parent` lines,
    //  then `author`, then `committer`.  So MERGE parents must be
    //  emitted HERE (consecutive with the first-parent line, before
    //  author).  `foster` is a beagle-only header git doesn't know — it
    //  rides AFTER `committer` (like `gpgsig`); emitting it before
    //  `author` breaks the required parent→author adjacency and git
    //  fsck rejects the object ("missingAuthor: expected 'author'
    //  line").  Walk oldest → newest within each class to preserve row
    //  order.
    sniff_pe pent[64];
    u32 n_pent = 0;
    (void)SNIFFAtPatchEntries(pent, 64, &n_pent);
    for (u32 i = 0; i < n_pent; i++) {
        if (pent[i].shape != 3 /* MERGE */) continue;
        a_cstr(mpar_label, "parent ");
        u8bFeed(com, mpar_label);
        a_pad(u8, fhex, 40);
        a_rawc(fraw, pent[i].sha);
        HEXu8sFeedSome(fhex_idle, fraw);
        u8bFeed(com, u8bDataC(fhex));
        u8bFeed1(com, '\n');
    }

    //  Wall-clock seconds for the commit body — sourced from the
    //  monotonicity-guarded ts already stamped on ctx, so the commit
    //  object, the post ULOG row, and the on-disk file mtimes all
    //  agree (per VERBS.md §POST "Wall-clock guard").
    a_pad(u8, tsbuf, 64);
    int tslen = snprintf((char *)u8bIdleHead(tsbuf), u8bIdleLen(tsbuf),
                         " %lld +0000\n",
                         (long long)ctx.stamp_tv.tv_sec);
    if (tslen > 0) u8bFed(tsbuf, (size_t)tslen);
    a_dup(u8c, ts_s, u8bDataC(tsbuf));

    a_cstr(auth_label, "author ");
    u8bFeed(com, auth_label);
    u8bFeed(com, author);
    u8bFeed(com, ts_s);

    a_cstr(comm_label, "committer ");
    u8bFeed(com, comm_label);
    u8bFeed(com, author);
    u8bFeed(com, ts_s);

    //  `foster <sha>` headers from SQUASH / REBASE1 patch rows — a
    //  beagle-only header, so it rides after `committer` (still inside
    //  the header block, before the blank line) where git treats
    //  unknown headers the way it treats `gpgsig`.  Row order preserved.
    for (u32 i = 0; i < n_pent; i++) {
        if (pent[i].shape == 2 /* CHERRY */ ||
            pent[i].shape == 3 /* MERGE  */) continue;
        a_cstr(fos_label, "foster ");
        u8bFeed(com, fos_label);
        a_pad(u8, fhex, 40);
        a_rawc(fraw, pent[i].sha);
        HEXu8sFeedSome(fhex_idle, fraw);
        u8bFeed(com, u8bDataC(fhex));
        u8bFeed1(com, '\n');
    }

    u8bFeed1(com, '\n');
    u8bFeed(com, message);
    u8bFeed1(com, '\n');

    //  Append `picked: <sha>` trailers for any cherry-pick entries
    //  in the patch chain (VERBS.md §POST).  One blank line between
    //  message body and trailers so a downstream reader can split
    //  them cleanly.
    {
        b8 any_picked = NO;
        for (u32 i = 0; i < n_pent; i++) {
            if (pent[i].shape != 2 /* CHERRY */) continue;
            if (!any_picked) {
                u8bFeed1(com, '\n');
                any_picked = YES;
            }
            a_cstr(pkl, "picked: ");
            u8bFeed(com, pkl);
            a_pad(u8, phex, 40);
            a_rawc(praw, pent[i].sha);
            HEXu8sFeedSome(phex_idle, praw);
            u8bFeed(com, u8bDataC(phex));
            u8bFeed1(com, '\n');
        }
    }

    //  11. Feed pack: commit first.
    a_dup(u8c, com_body, u8bData(com));
    ok64 fo = KEEPPackFeed(&p, DOG_OBJ_COMMIT, com_body, 0, sha_out);
    if (fo != OK) {
        KEEPPackClose(&p);
        return fo;
    }

    //  12. Feed all rebuilt trees in reverse-of-post-order — i.e.,
    //      root first, descendants after.  post_build_tree pushed
    //      bodies in DFS post-order (children before their parent),
    //      because each parent's body needs its children's SHAs and
    //      can only be sealed once they've been hashed.  Reversing
    //      that emission is a valid topological *parent-first* order:
    //      every ancestor precedes its descendants, exactly what the
    //      keeper-side path-hash propagation in spot needs to greedily
    //      stamp `obj_hl → path_hash` on every leaf as it streams in.
    //
    //      Implementation: forward-walk tree_bodies once to collect
    //      record offsets (records are <u32 len, body>), then iterate
    //      offsets[] in reverse and feed pack.
    if (have_root) {
        a_dup(u8c, whole, u8bDataC(tree_bodies));
        u32 wlen = (u32)u8csLen(whole);
        a_carve(u32, offs, tree_count > 0 ? tree_count : 1);
        a_dup(u8c, scan, whole);
        while (!u8csEmpty(scan)) {
            (void)u32bFeed1(offs, (u32)(scan[0] - whole[0]));
            u32 tlen = 0;
            if (u8sDrain32(scan, &tlen) != OK) break;
            if (u8csUsed(scan, tlen) != OK) break;
        }
        u32 nrec = (u32)u32bDataLen(offs);
        u32 *off_base = u32bDataHead(offs);
        for (u32 i = nrec; i > 0; i--) {
            u32 from = off_base[i - 1];
            u32 till = (i < nrec) ? off_base[i] : wlen;
            u8cs rec = {};
            if (u8csSub(whole, rec, from, till) != OK) continue;
            u32 tlen = 0;
            if (u8sDrain32(rec, &tlen) != OK) continue;
            sha1 tsha_dummy = {};
            //  TODO(delta-trees): pass the parent commit's same-path
            //  tree SHA as `base_hashlet60` instead of 0.  A typical
            //  commit changes ≤3 entries per touched directory, so a
            //  delta against the parent tree shrinks each new tree
            //  ~95 %.  Source for the base SHA: extend
            //  keeper/WALK.c:treeulog_visit to also emit DIR rows
            //  (currently it skips them, line ~358), then post_build_tree
            //  can pluck the old subtree SHA per `subprefix` out of `bu`
            //  on its way down — same plumbing as `old_sha` already does
            //  for blobs at line 2371.  No extra walk needed.
            ok64 to = KEEPPackFeed(&p, DOG_OBJ_TREE, rec,
                                   0, &tsha_dummy);
            if (to != OK) {
                KEEPPackClose(&p);
                return to;
            }
        }
    }

    //  13. Feed all new blobs.  Drains `add` decisions; for each row
    //      reads the wt file (mmap for regular/exec, readlink for
    //      symlink) and feeds the bytes into the pack.  Delta base:
    //      when the row carries an `&<old_sha>` chain, use that sha
    //      as the OFS/REF_DELTA target so small edits ride bsdiff
    //      instead of a fresh zlib-of-everything.
    {
        a_dup(u8c, scan, u8bData(ctx.decisions));
        while (!u8csEmpty(scan)) {
            ulogrec drec = {};
            ok64 dr = ULOGu8sDrain(scan, &drec);
            if (dr == NODATA) break;
            if (dr != OK) continue;
            if (ok64stem(drec.verb) != POST_V_ADD) continue;

            u8cs path = {drec.uri.path[0], drec.uri.path[1]};
            u16  mode = post_kind_to_mode(ok64Lit(drec.verb, 0));
            sha1 old_sha = {};
            b8   has_old = post_decision_old_sha(&drec.uri, &old_sha);

            a_path(fp);
            if (SNIFFFullpath(fp, reporoot, path) != OK) continue;

            //  Read the bytes.  Symlinks via readlink into the
            //  iteration-scoped `target` stack buffer (mmap doesn't
            //  compose); regular/exec via FILEMapRO.  `body` borrows
            //  whichever source — both outlive the pack feed below.
            a_pad(u8, target, 1024);
            u8bp mapped = NULL;
            u8cs body = {};
            if (mode == 0120000) {
                if (FILEReadLink(target, $path(fp)) != OK) continue;
                a_dup(u8c, tgt_data, u8bData(target));
                body[0] = tgt_data[0];
                body[1] = tgt_data[1];
            } else {
                ok64 mo = FILEMapRO(&mapped, $path(fp));
                if (mo != OK) continue;
                body[0] = u8bDataHead(mapped);
                body[1] = u8bIdleHead(mapped);
            }

            //  Refuse to commit any file containing PATCH's
            //  conflict-marker triple (open + mid + close, each
            //  four chars; see PATCH.c for the exact byte
            //  pattern).  An unattended `patch && post` chain
            //  stops here with POSTCFLCT before recording a
            //  half-merged commit (VERBS.md §PATCH "Reporting"
            //  — conflict-loud rule).  Lone open or close (prose
            //  mentions) doesn't trigger — see
            //  `SNIFFHasConflictMarker` for the exact predicate.
            //  `--force` skips the scan as an escape hatch for
            //  false positives (string literals describing the
            //  marker shape, etc.).  This comment deliberately
            //  avoids writing the triple inline because the
            //  predicate doesn't know it's reading a comment and
            //  would refuse to commit this file (POST.c) on its
            //  own self-description.
            if (!force) {
                if (SNIFFHasConflictMarker(body)) {
                    fprintf(stderr,
                        "sniff: post: refusing — conflict "
                        "marker in tracked file " U8SFMT " "
                        "(re-run with --force to override)\n",
                        u8sFmt(path));
                    if (mapped) FILEUnMap(mapped);
                    KEEPPackClose(&p);
                    return POSTCFLCT;
                }
            }
            u64 base_hl = has_old ? WHIFFHashlet60(&old_sha) : 0;
            sha1 bsha = {};
            ok64 bo = KEEPPackFeed(&p, DOG_OBJ_BLOB, body,
                                   base_hl, &bsha);
            if (mapped) FILEUnMap(mapped);
            if (bo != OK) {
                KEEPPackClose(&p);
                return bo;
            }
        }
    }

    call(KEEPPackClose, &p);

    //  13b. Phase-2 promote: rebase the just-built commit onto the
    //       live REFS tip when the branch diverged out from under us,
    //       then cascade-rebase every descendant branch onto the new
    //       tip.  Same-branch case only.
    //
    //       Invariant: on entry needs_rebase ⇒ has_expected_tip and
    //       has_parent (the early pre-flight only sets needs_rebase
    //       when both were observed).  GRAFRebase replays parent..sha_out
    //       onto expected_tip_sha; emitted objects feed straight into a
    //       fresh pack.  GRAFCNFL → propagate, leaving orphan objects
    //       in the pack (REFS unchanged ⇒ they are unreachable).
    //
    //       Cascade: after the same-branch rebase closes its pack, a
    //       third pack opens for the descendant walk so the just-rebased
    //       tip is visible to KEEPGetExact.  Each descendant branch is
    //       replayed via GRAFRebase, the new tips are staged in
    //       cascade_ctx.recs, and post_cascade_persist commits the
    //       REFSCompareAndAppend writes after cur's REFS update.
    //
    //       TODO(spec): cross-branch promote (target_branch != cur's
    //       baseline branch) — `?..` auto-sync, `?<absolute>` sibling/
    //       cousin promote, create-on-miss leaf, trailing-slash basename
    //       reuse.  The dispatch table from VERBS.md §POST stays
    //       deferred.  Today cross-branch non-ff still reports SNIFFNOFF
    //       via the early pre-flight on the OTHER branch's REFS row,
    //       since needs_rebase is gated by the baseline-branch lookup
    //       above.
    cascade_ctx casc = {};
    if (needs_rebase) {
        keep_pack p2 = {};
        call(KEEPPackOpen, &p2);
        p2.strict_order = NO;
        post_rebase_ctx rctx = {.p = &p2};
        ok64 rb = GRAFRebase(&parent, &expected_tip_sha, sha_out,
                             post_rebase_emit_cb, &rctx);
        ok64 cl2 = KEEPPackClose(&p2);
        if (rb != OK) {
            fprintf(stderr,
                    "sniff: post: rebase aborted (%s)\n",
                    rb == GRAFCNFL ? "merge conflict" : "error");
            return rb;
        }
        if (cl2 != OK) return cl2;
        if (rctx.have_last_commit) {
            *sha_out = rctx.last_commit_sha;
        } else {
            //  Trivial rebase fast-path: parent..child was empty, so
            //  the rebased tip is base_new itself.
            *sha_out = expected_tip_sha;
        }

        //  --- Cascade rebase for descendants of cur's branch ---
        //  When the same-branch rebase rewrote cur's stack, every
        //  descendant branch that forked off `expected_tip_sha` (or
        //  earlier) now has a stale fork point.  Open a third pack for
        //  the cascade so the just-rebased ?cur tip is visible to
        //  KEEPGetExact (the rebase commits live in p2, which must be
        //  closed before they are indexed).
        a_dup(u8c, branch_view, u8bData(brbuf));
        sha1 br_new = *sha_out;
        keep_pack p3 = {};
        call(KEEPPackOpen, &p3);
        //  Cascade record arena — BASS-carved, lives to function end
        //  (consumed by post_cascade_persist after the REFS update).
        a_carve(u8, casc_arena, CASCADE_MAX * 256);
        ((u8 **)casc.arena)[0] = casc_arena[0];
        ((u8 **)casc.arena)[1] = casc_arena[1];
        ((u8 **)casc.arena)[2] = casc_arena[2];
        ((u8 **)casc.arena)[3] = casc_arena[3];
        p3.strict_order = NO;
        casc.p = &p3;
        ok64 cw = post_cascade_walk(&casc, branch_view,
                                    &expected_tip_sha, &br_new);
        ok64 cl3 = KEEPPackClose(&p3);
        if (cw != OK) {
            fprintf(stderr,
                    "sniff: post: cascade aborted (%s)\n",
                    cw == GRAFCNFL ? "merge conflict in descendant"
                                   : "error");
            return cw;
        }
        if (cl3 != OK) return cl3;
    }

    //  14. Advance keeper REFS for the be-branch the wt is currently
    //      on via REFSCompareAndAppend.  Atomic check-and-set on the
    //      *expected* tip (the REFS row we read at pre-flight time, or
    //      empty when no row existed yet — a fresh branch).  Concurrent
    //      posters who advanced the branch since pre-flight see REFSCAS
    //      and surface it to the caller.
    a_pad(u8, out_hex, 40);
    {
        a_rawc(osha, *sha_out);
        HEXu8sFeedSome(out_hex_idle, osha);
    }
    {
        a_path(keepdir);
        call(HOMEBranchDir, k->h, keepdir, NULL);
        a_pad(u8, keybuf, 128);
        u8bFeed1(keybuf, '?');
        a_dup(u8c, branch, u8bData(brbuf));
        if (!u8csEmpty(branch)) u8bFeed(keybuf, branch);
        a_dup(u8c, refkey, u8bData(keybuf));

        a_dup(u8c, val, u8bDataC(out_hex));

        a_pad(u8, exp_hex, 40);
        a_cstr(empty_s, "");
        u8cs expected = {empty_s[0], empty_s[1]};
        if (has_expected_tip) {
            a_rawc(esha, expected_tip_sha);
            HEXu8sFeedSome(exp_hex_idle, esha);
            expected[0] = u8bDataHead(exp_hex);
            expected[1] = u8bIdleHead(exp_hex);
        }
        ok64 cr = REFSCompareAndAppend($path(keepdir), refkey, expected, val);
        if (cr == REFSCAS) {
            fprintf(stderr,
                    "sniff: post: REFS for `?" U8SFMT "` advanced "
                    "concurrently — retry\n",
                    u8sFmt(branch));
            //  Best-effort: don't undo the pack feed.  Caller may retry
            //  POST against the new tip.  The per-commit buffers
            //  (decisions, tree_bodies, cascade arena) are BASS-carved
            //  and rewind automatically when POSTCommit returns.
            if (casc.n > 0) (void)post_cascade_persist(&casc);
            return REFSCAS;
        }
    }

    //  Cascade REFS persistence: write descendant branches' new tips
    //  AFTER cur's REFS update succeeded.  Best-effort on individual
    //  CAS races (logged inside post_cascade_persist).
    if (casc.n > 0) (void)post_cascade_persist(&casc);

    //  14b. Tag label: when the caller passed a tag-shaped target
    //  (single segment, no slash — see DOGRefIsBranch), the commit
    //  landed on cur (above) and we now point the tag at the new
    //  tip.  Tags are re-pointable freely — no FF/CAS gate.  Mirrors
    //  PUTSetLabel (REFSAppendVerb with `post` verb).
    if ($ok(target_branch) && !u8csEmpty(target_branch) &&
        !target_is_branch) {
        a_path(keepdir);
        call(HOMEBranchDir, k->h, keepdir, NULL);
        a_pad(u8, tagkey, 128);
        u8bFeed1(tagkey, '?');
        u8bFeed(tagkey, target_branch);
        a_dup(u8c, tagref, u8bData(tagkey));
        a_dup(u8c, val,    u8bDataC(out_hex));
        ron60 vpost = SNIFFAtVerbPost();
        (void)REFSAppendVerb($path(keepdir), vpost, tagref, val);
    }

    //  15. Append `post` ULOG row with stamp ts; futimens written
    //      files so they become clean under the new stamp.  Canonical
    //      at-log shape: `?<branch>#<curhash>` — query is the
    //      be-branch (empty for trunk), fragment is the new tip sha.
    //      Mirrors the REFS row format so readers walk the same shape.
    uri urow = {};
    {
        a_dup(u8c, branch, u8bData(brbuf));
        urow.query[0] = u8bDataHead(brbuf);
        urow.query[1] = u8bIdleHead(brbuf);
        (void)branch;
    }
    {
        a_dup(u8c, h, u8bDataC(out_hex));
        urow.fragment[0] = h[0];
        urow.fragment[1] = h[1];
    }

    //  Use the single per-commit stamp the decision rows already
    //  carry — keeps the post ULOG row, the decision rows, and the
    //  on-disk file stamps all in lockstep.
    ron60 verb = SNIFFAtVerbPost();
    ok64 ar = SNIFFAtAppendAt(ctx.stamp_ts, verb, &urow);
    (void)ar;

    //  Stamp only `add` files (drain add decisions).  KEEP files keep
    //  their previous get/post stamp — re-stamping them is redundant.
    post_walk_decisions(&ctx, POST_VM_ADD, post_drain_stamp_cb, NULL);

    //  16. Per-file change report (ULOG status lines; colour via
    //      HUNKMode — see post_drain_mad_cb).  Interactive (plain /
    //      color) routes to stderr so stdout stays clean; TLV (capture)
    //      mode routes to stdout so a parent `be post` recursing into
    //      this submodule can capture the hunk stream and relay it with
    //      a path prefix (BERelaySub).
    {
        int report_fd = (HUNKMode == HUNKOutTLV) ? STDOUT_FILENO
                                                 : STDERR_FILENO;
        post_mad_ctx mad = {.fd = report_fd, .changed = 0};
        post_walk_decisions(&ctx, POST_VM_UNLINK | POST_VM_ADD,
                            post_drain_mad_cb, &mad);
    }

    //  17. Per-commit scratch (decisions, tree_bodies, cascade arena)
    //      is BASS-carved and rewinds when POSTCommit returns.
    fprintf(stderr, "sniff: commit " U8SFMT "\n",
            u8sFmt(u8bDataC(out_hex)));
    done;
}
