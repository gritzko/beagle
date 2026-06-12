//  PUT: append `put <path>` rows to the ULOG and stamp each staged
//  file with the row's ts.  A file's mtime then points back into the
//  log — POST classifies provenance via `lstat → ron60 → row`.
//
//  Bare `be put` walks the baseline tree (tracked paths only) and
//  emits a row+stamp for each tracked file whose mtime ∉ stamp-set.
//  Untracked paths are NEVER staged by the bare form — the user
//  names them explicitly with `be put <path>`.  Empty walk →
//  PUTNONE.
//
//  Per-uri `be put <path>` validates each path:
//    * missing on disk        → PUTNONE (caller can't stage what
//                                isn't there)
//    * mtime ∈ get/post stamp → PUTNONE (already baseline-clean;
//                                re-stamping under `put` would shift
//                                provenance for no semantic gain)
//    * otherwise              → append row, stamp file
//
#include "PUT.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "abc/BUF.h"
#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "dog/DOG.h"
#include "dog/DPATH.h"
#include "dog/git/GIT.h"
#include "keeper/KEEP.h"
#include "keeper/REFS.h"
#include "keeper/WALK.h"
#include "dog/git/SHA1.h"

#include "AT.h"
#include "CLASS.h"
#include "POST.h"
#include "SUBS.h"

// --- Bare-walk callback (baseline-tree visitor) ---

typedef struct {
    u8cs   reporoot;
    ron60  ts;            // bumped after each emit so rows stay strictly increasing
    ron60  verb_put;
    ron60  baseline_ts;   // ts of the most recent get/post — used to
                          // re-stamp files that hash-match baseline so
                          // future mtime fast-paths skip them.
    u32    emitted;
    ok64   err;
} put_walk_ctx;

//  Per-tracked-path:
//    1. lstat — skip if absent.
//    2. mtime ∈ stamp-set → clean (fast path).
//    3. Otherwise mmap+hash and compare to the baseline blob sha:
//       * equal → file content matches baseline; re-stamp with
//         baseline_ts so the mtime fast-path picks it up next time.
//       * differs → emit a put row and stamp with put ts.
//
//  mtimes are an optimization; the SHA-1 comparison is the actual
//  dirtiness test.  Without this, a `.be/wtlog` carried over from
//  another checkout (where every wt mtime is foreign) would put-stage
//  the entire baseline tree even though nothing actually changed.
static ok64 put_visit_tracked(u8cs path, u8 kind, u8cp esha, u8cs blob,
                              void0p vctx) {
    sane(vctx);
    (void)blob;
    put_walk_ctx *c = (put_walk_ctx *)vctx;

    //  Directories don't carry content; subtree walk handles them.
    if (kind == WALK_KIND_DIR) return OK;
    //  Submodules / gitlinks: nothing to put.  POST keeps them via
    //  the gitlink-pass-through rule.
    if (kind == WALK_KIND_SUB) return WALKSKIP;
    //  Meta paths (.be/wtlog / .be/* / .git*) leak into legacy trees
    //  but must never be re-staged.  Skip even when present on the
    //  tracked side — POST will drop them from the new tree too.
    if (SNIFFSkipMeta(path)) return OK;

    a_path(fp);
    if (SNIFFFullpath(fp, c->reporoot, path) != OK) return OK;

    filestat fs = {};
    ok64 lo = FILELStat(&fs, $path(fp));
    if (lo == FILENONE) return OK;    // vanished mid-walk
    if (lo != OK) return lo;             // permissions etc — propagate
    ron60 mr = fs.mtime;

    //  Fast path: mtime equals some `get` or `post` row's ts → sniff
    //  wrote this file as part of materialising a tree, the user
    //  hasn't touched it since.  Either the row's tree at this path
    //  IS the current baseline blob (typical case) or it differs in
    //  a way invisible to bare put — staging a baseline-equal file
    //  again is a no-op POST will skip, so suppressing here is safe.
    //  put / patch / older-mod stamps don't carry the "user untouched"
    //  guarantee, so they fall through to the SHA compare.
    if (SNIFFAtKnown(mr)) {
        ron60 ow_verb = 0;
        uri ow_u = {};
        if (SNIFFAtRowAtTs(mr, &ow_verb, &ow_u) == OK) {
            ron60 vg = SNIFFAtVerbGet();
            ron60 vp = SNIFFAtVerbPost();
            if (ow_verb == vg || ow_verb == vp) return OK;
        }
    }

    //  Slow path: hash on-disk content and compare to baseline blob
    //  sha.  Symlinks resolve via readlink; regular/exec via mmap.
    sha1 disk_sha = {};
    sha1 base_sha = {};
    sha1Mv(&base_sha, (sha1cp)esha);

    if (kind == WALK_KIND_LNK) {
        a_pad(u8, tgt, 1024);
        if (FILEReadLink(tgt, $path(fp)) != OK) return OK;
        KEEPObjSha(&disk_sha, DOG_OBJ_BLOB, u8bDataC(tgt));
    } else {
        u8bp mapped = NULL;
        ok64 mo = FILEMapRO(&mapped, $path(fp));
        if (mo != OK) return OK;            // can't read → leave alone
        KEEPObjSha(&disk_sha, DOG_OBJ_BLOB, u8bDataC(mapped));
        FILEUnMap(mapped);
    }

    if (sha1Eq(&disk_sha, &base_sha)) {
        //  Content matches baseline.  Re-stamp with baseline_ts so the
        //  next mtime check fast-paths this file.  No put row.
        if (c->baseline_ts != 0)
            (void)SNIFFAtStampPath(fp, c->baseline_ts);
        return OK;
    }

    uri urow = {};
    urow.path[0] = path[0];
    urow.path[1] = path[1];
    ok64 ar = SNIFFAtAppendAt(c->ts, c->verb_put, &urow);
    if (ar != OK) { c->err = ar; return ar; }

    (void)SNIFFAtStampPath(fp, c->ts);
    c->emitted++;
    c->ts++;        // strict-increase invariant for the next row

    //  Report each staged file as a ULOG status hunk on stdout (like
    //  GET / PATCH): the user sees what `be put` staged, and a parent
    //  `be put` recursing into this submodule captures the TLV stream
    //  and relays it path-prefixed (BERelaySub).  The `staged N row(s)`
    //  stderr line stays as a summary.
    {
        ulogrec rep = {.ts = 0, .verb = c->verb_put};
        u8csMv(rep.uri.path, path);
        (void)ULOGPrintStatusLine(&rep);
    }
    return OK;
}

//  Resolve baseline tree-sha from sniff's at-log.  ULOGNONE on a
//  fresh (no-baseline) log; on OK the 20-byte tree sha is in
//  *tree_sha_out.
static ok64 put_baseline_tree(sha1 *tree_sha_out) {
    sane(tree_sha_out);
    b8 have = NO;
    ok64 br = SNIFFAtBaselineTreeSha(YES, tree_sha_out, &have);
    if (br != OK) return br;
    return have ? OK : ULOGNONE;
}

// --- Per-path classification via SNIFFClassify ----------------------

typedef struct {
    u8cs        raw;            // bytes from user URI (rel path, normalised)
    b8          seen;
    b8          stage;
    char const *skip_reason;
    ron60       mtime;          // wt mtime, captured from the merge
} put_req;

typedef struct {
    put_req *reqs;
    u32      n;
    ron60    verb_get;
    ron60    verb_post;
} put_ctx;

static ok64 put_classify_step(class_step const *step, void *ctx_) {
    put_ctx *w = (put_ctx *)ctx_;
    for (u32 j = 0; j < w->n; j++) {
        if (!u8csEq(w->reqs[j].raw, step->path)) continue;
        put_req *r = &w->reqs[j];
        r->seen = YES;
        if (step->kind == CLASS_BASE_ONLY) {
            r->skip_reason = "does not exist";
            return OK;
        }
        //  WT_ONLY or BOTH — file is on disk.
        if (step->wt_rec) r->mtime = step->wt_rec->ts;
        if (step->kind == CLASS_BOTH && SNIFFAtKnown(r->mtime)) {
            ron60 ow_verb = 0;
            uri ow_u = {};
            if (SNIFFAtRowAtTs(r->mtime, &ow_verb, &ow_u) == OK &&
                (ow_verb == w->verb_get ||
                 ow_verb == w->verb_post)) {
                r->skip_reason = "is unchanged";
                return OK;
            }
        }
        r->stage = YES;
        return OK;
    }
    return OK;
}

// --- Dir-form expansion --------------------------------------------
//
//  `be put <dir>/` (trailing slash): expand to one per-file `put` row
//  per path.  https://replicated.wiki/html/wiki/PUT.html §PUT contract:
//
//      tracked dir   (any baseline entry under prefix) → stage every
//                    tracked-dirty file, skip untracked siblings
//      untracked dir (no baseline entry under prefix) → stage every
//                    non-meta file under it
//
//  We previously emitted a single dir-prefix row and deferred
//  expansion to POST.  That left status (`be`) unable to surface the
//  staged files (CLASS.c skips dir-prefix rows for the per-file
//  classifier), so users saw `0 put / 0 mod` despite their explicit
//  `be put dir/`, and a fresh row-per-call broke idempotence.

//  Stream rows directly into the .be/wtlog ULOG from the classify
//  callback — no in-memory path-set intermediate.  The merge yields
//  paths in lex order; SNIFFAtAppendAt only requires monotonic ts,
//  which `*ts_io` carries forward across calls.
typedef struct {
    u8cs    prefix;
    u8cs    reporoot;
    ron60  *ts_io;
    u32    *emitted_io;
    ron60   verb_put;
    b8      saw_tracked;   // YES iff any baseline (tracked) entry under prefix
    ok64    err;
} dir_collect_ctx;

static ok64 dir_collect_step(class_step const *step, void *vctx) {
    sane(step && vctx);
    dir_collect_ctx *c = (dir_collect_ctx *)vctx;
    u8cs path = {step->path[0], step->path[1]};
    if (!u8csHasPrefix(path, c->prefix)) return OK;

    //  A baseline entry under the prefix means this is a TRACKED dir; an
    //  empty expansion then is genuinely "unchanged".  No baseline entry
    //  → the dir is untracked, and an empty expansion is "no files to
    //  stage" — distinct messages downstream (SUBS-014).
    if (step->kind == CLASS_BOTH || step->kind == CLASS_BASE_ONLY)
        c->saw_tracked = YES;

    //  https://replicated.wiki/html/wiki/PUT.html §PUT dir-form contract:
    //    BOTH    + mtime ∈ stamp-set   → settled, skip
    //    BOTH    + otherwise           → stage (tracked-and-dirty)
    //    WT_ONLY                       → stage (untracked sibling)
    //    BASE_ONLY                     → skip (gone from disk; staging
    //                                    a vanished tracked path as a
    //                                    deletion is `be delete`'s job,
    //                                    not put's — put never removes
    //                                    paths from the next commit).
    //
    //  Idempotence: any stamp-set match (get/post or put) counts as
    //  settled — re-staging would just shift ts forward for no
    //  semantic gain.  `be put dir/` twice in a row emits zero rows
    //  on the second call.
    b8 stage = NO;
    if (step->kind == CLASS_BOTH) {
        ron60 mr = step->wt_rec ? step->wt_rec->ts : 0;
        if (!(mr != 0 && SNIFFAtKnown(mr))) stage = YES;
    } else if (step->kind == CLASS_WT_ONLY) {
        stage = YES;
    }
    if (!stage) return OK;

    a_path(fp);
    if (SNIFFFullpath(fp, c->reporoot, path) != OK) return OK;

    uri urow = {};
    urow.path[0] = path[0];
    urow.path[1] = path[1];
    ok64 ar = SNIFFAtAppendAt(*c->ts_io, c->verb_put, &urow);
    if (ar != OK) { c->err = ar; return ar; }
    (void)SNIFFAtStampPath(fp, *c->ts_io);
    (*c->ts_io)++;
    (*c->emitted_io)++;
    return OK;
}

// --- Bare-put move auto-pair ---
//
//  After a system `mv` (or any rename outside `be`), the wt has one
//  tracked path missing on disk and one untracked path of identical
//  content.  Bare `be put` finds these pairs by sha and writes one
//  `put <old>#<new>` row per pair.  Refuses PUTAMBIG when the
//  pairing isn't strictly 1:1 (>1 candidate either direction with
//  the same content sha), so the user falls back to the explicit
//  form to disambiguate.  Only file-level moves; CLASS rows whose
//  step already carries a put/del intent are skipped so the bare
//  sweep never double-counts a path the user named explicitly.
//
//  Cap of PUT_MV_MAX candidates per side keeps the O(nb·nw) pairing
//  trivial; real workflows have a handful of moves per `be put`.

#define PUT_MV_MAX     256
#define PUT_MV_BUFSZ   (1UL << 16)   // 64 KiB path-byte arena

typedef struct {
    u8cs path;     // slice into mv_state.pathbuf (mmap-backed, stable)
    sha1 sha;
} mv_entry;

typedef struct {
    mv_entry base[PUT_MV_MAX];   // tracked paths missing from disk
    mv_entry wt  [PUT_MV_MAX];   // untracked paths on disk
    u32      nb, nw;
    b8       overflow;
    Bu8      pathbuf;            // owned; freed by put_detect_moves
} mv_state;

//  Append `path`'s bytes into `pathbuf` and fill `out` with the
//  interned slice.  On overflow returns the underlying error and
//  leaves `out` empty (caller drops the entry).
static ok64 mv_intern(Bu8 pathbuf, u8cs path, u8cs out) {
    out[0] = u8bIdleHead(pathbuf);
    ok64 fo = u8bFeed(pathbuf, path);
    if (fo != OK) { out[0] = NULL; out[1] = NULL; return fo; }
    out[1] = u8bIdleHead(pathbuf);
    return OK;
}

static ok64 mv_collect_cb(class_step const *step, void *vctx) {
    mv_state *s = (mv_state *)vctx;
    u8cs path = {step->path[0], step->path[1]};
    if (u8csEmpty(path))                return OK;
    if (SNIFFSkipMeta(path))            return OK;
    //  Skip rows the user already named via explicit put/del; the
    //  auto-pair only catches the system-`mv` case.
    if (step->put_rec || step->del_rec) return OK;

    if (step->kind == CLASS_BASE_ONLY) {
        if (s->nb >= PUT_MV_MAX || step->base_rec == NULL) {
            if (s->nb >= PUT_MV_MAX) s->overflow = YES;
            return OK;
        }
        //  Tree-ULog rows carry the leaf blob sha in the URI's
        //  fragment slot (40 hex).  Skip non-leaf rows (gitlinks,
        //  malformed) without complaint — they can't anchor a move.
        u8cs frag = {step->base_rec->uri.fragment[0],
                     step->base_rec->uri.fragment[1]};
        if (u8csLen(frag) != 40) return OK;
        sha1 sh = {};
        a_raw(shb, sh);
        a_dup(u8c, hex, frag);
        if (HEXu8sDrainSome(shb, hex) != OK) return OK;
        u8cs slot = {};
        if (mv_intern(s->pathbuf, path, slot) != OK) {
            s->overflow = YES; return OK;
        }
        u8csMv(s->base[s->nb].path, slot);
        s->base[s->nb].sha = sh;
        s->nb++;
    } else if (step->kind == CLASS_WT_ONLY) {
        if (s->nw >= PUT_MV_MAX) { s->overflow = YES; return OK; }
        u8cs slot = {};
        if (mv_intern(s->pathbuf, path, slot) != OK) {
            s->overflow = YES; return OK;
        }
        u8csMv(s->wt[s->nw].path, slot);
        //  sha left zero — filled by mv_hash_wt below.
        s->nw++;
    }
    return OK;
}

//  Hash each WT_ONLY candidate as a git blob; entries we can't hash
//  keep their zero sha and won't pair with anything real.
static void mv_hash_wt(mv_state *s, u8cs reporoot) {
    for (u32 i = 0; i < s->nw; i++) {
        a_dup(u8c, rel, s->wt[i].path);
        a_path(fp);
        if (SNIFFFullpath(fp, reporoot, rel) != OK) continue;
        filestat fs = {};
        if (FILELStat(&fs, $path(fp)) != OK) continue;
        if (fs.kind == FILE_KIND_LNK) {
            a_pad(u8, tgt, 4096);
            if (FILEReadLink(tgt, $path(fp)) != OK) continue;
            KEEPObjSha(&s->wt[i].sha, DOG_OBJ_BLOB, u8bDataC(tgt));
        } else if (fs.kind == FILE_KIND_REG) {
            if (fs.size == 0) {
                u8cs empty = {NULL, NULL};
                KEEPObjSha(&s->wt[i].sha, DOG_OBJ_BLOB, empty);
            } else {
                u8bp m = NULL;
                if (FILEMapRO(&m, $path(fp)) != OK) continue;
                u8cs body = {u8bDataHead(m), u8bIdleHead(m)};
                KEEPObjSha(&s->wt[i].sha, DOG_OBJ_BLOB, body);
                FILEUnMap(m);
            }
        }
    }
}

typedef struct { u32 b_idx, w_idx; } mv_pair;

//  Pair every base candidate against wt candidates by sha equality.
//  Refuses PUTAMBIG when a base sha matches >1 wt entry (or vice
//  versa) — the user has to disambiguate via the explicit form.
//  Output: `pairs[0..*nout)` valid on OK return.
static ok64 mv_pair_unique(mv_state const *s, mv_pair *pairs, u32 *nout) {
    *nout = 0;
    for (u32 i = 0; i < s->nb; i++) {
        u32 match = UINT32_MAX;
        u32 nmatch = 0;
        for (u32 j = 0; j < s->nw; j++) {
            if (sha1Eq(&s->base[i].sha, &s->wt[j].sha)) {
                match = j; nmatch++;
            }
        }
        if (nmatch == 0) continue;
        if (nmatch > 1)  return PUTAMBIG;
        //  Symmetry: the wt entry must match only this base.
        u32 base_matches = 0;
        for (u32 k = 0; k < s->nb; k++) {
            if (sha1Eq(&s->base[k].sha, &s->wt[match].sha)) base_matches++;
        }
        if (base_matches > 1) return PUTAMBIG;
        pairs[*nout].b_idx = i;
        pairs[*nout].w_idx = match;
        (*nout)++;
    }
    return OK;
}

//  Sweep: collect candidates via SNIFFClassify, hash wt-side, pair
//  1:1 by content sha, emit one `put <old>#<new>` row per pair, and
//  stamp the dest file with the row's ts so subsequent bare scans
//  fast-path it via SNIFFAtKnown.  Returns PUTAMBIG without writing
//  any row when the pairing isn't unique.
static ok64 put_detect_moves(u8cs reporoot, ron60 *ts_io,
                             u32 *emitted_io, ron60 verb_put) {
    sane(ts_io && emitted_io);
    mv_state s = {};
    //  Path scratch from BASS (rewinds at return); mv_collect_cb's
    //  `s.base`/`s.wt` slices borrow into it, so it must outlive the
    //  classify + pairing below — carved here, in the owning frame.
    a_carve(u8, pathbuf, PUT_MV_BUFSZ);
    ((u8 **)s.pathbuf)[0] = pathbuf[0];
    ((u8 **)s.pathbuf)[1] = pathbuf[1];
    ((u8 **)s.pathbuf)[2] = pathbuf[2];
    ((u8 **)s.pathbuf)[3] = pathbuf[3];
    ok64 cr = SNIFFClassify(mv_collect_cb, &s);
    if (cr != OK)        return cr;
    if (s.nb == 0 || s.nw == 0) return OK;

    mv_hash_wt(&s, reporoot);

    mv_pair pairs[PUT_MV_MAX];
    u32 npairs = 0;
    ok64 po = mv_pair_unique(&s, pairs, &npairs);
    if (po != OK) return po;

    for (u32 i = 0; i < npairs; i++) {
        mv_entry const *bb = &s.base[pairs[i].b_idx];
        mv_entry const *ww = &s.wt  [pairs[i].w_idx];
        uri urow = {};
        urow.path[0]     = bb->path[0]; urow.path[1]     = bb->path[1];
        urow.fragment[0] = ww->path[0]; urow.fragment[1] = ww->path[1];
        ok64 ao = SNIFFAtAppendAt(*ts_io, verb_put, &urow);
        if (ao != OK) return ao;
        a_dup(u8c, dst_rel, ww->path);
        a_path(dstfp);
        if (SNIFFFullpath(dstfp, reporoot, dst_rel) == OK)
            (void)SNIFFAtStampPath(dstfp, *ts_io);
        (*ts_io)++;
        (*emitted_io)++;
    }
    done;
}

// --- Move-form (`be put <old>#<new>`) ---
//
//  Path slot = source, fragment slot = dest.  Trailing-slash dest is a
//  directory target — basename(src) gets appended.  Renames the file
//  on disk via FILERename and writes one `put` row with URI
//  `<old>#<final_new>` (fragment carries the resolved dest path, not a
//  content-locator — see sniff/AT.md §"Move-form put rows").  POST
//  consumes the row as "drop <old> from new tree, add <final_new>";
//  status renders it as one `mov` line per pair.

static ok64 put_move(u8cs src_raw, u8cs dst_in_raw, u8cs reporoot,
                     ron60 *ts_io, u32 *emitted_io, ron60 verb_put) {
    sane(ts_io && emitted_io);

    if (u8csEmpty(src_raw) || u8csEmpty(dst_in_raw)) return PUTNOSRC;
    if (SNIFFSkipMeta(src_raw) || SNIFFSkipMeta(dst_in_raw))
        return PUTMVMETA;

    //  Trailing-slash dest → directory target: append basename(src).
    //  Compose the final dest in a stack buffer; the resulting bytes
    //  outlive only this function — SNIFFAtAppendAt serializes the
    //  URI into the log before returning so the buffer can go away.
    a_pad(u8, dst_buf, FILE_PATH_MAX_LEN);
    b8 dst_is_dir = (*u8csLast(dst_in_raw) == '/');
    if (dst_is_dir) {
        u8cs src_base = {};
        PATHu8sBase(src_base, src_raw);
        if (u8csEmpty(src_base)) return PUTDSTBAD;
        call(u8bFeed,  dst_buf, dst_in_raw);
        call(u8bFeed,  dst_buf, src_base);
    } else {
        call(u8bFeed,  dst_buf, dst_in_raw);
    }
    a_dup(u8c, dst, u8bDataC(dst_buf));

    //  Resolve on-disk paths.
    a_path(src_fp);
    if (SNIFFFullpath(src_fp, reporoot, src_raw) != OK) return PUTNOSRC;
    a_path(dst_fp);
    if (SNIFFFullpath(dst_fp, reporoot, dst) != OK) return PUTDSTBAD;

    filestat src_fs = {}, dst_fs = {};
    ok64 src_lo = FILELStat(&src_fs, $path(src_fp));
    ok64 dst_lo = FILELStat(&dst_fs, $path(dst_fp));
    b8 src_here = (src_lo == OK);
    b8 dst_here = (dst_lo == OK);

    //  Two acceptable shapes: (a) rename in flight — src on disk, dst
    //  free; we perform the rename(2).  (b) claim — user already ran
    //  `mv` (or similar) before invoking `be put`, so dst already
    //  carries src's bytes and only the log row is missing.
    if (src_here && dst_here) return PUTDSTBAD;
    if (!src_here && !dst_here) return PUTNOSRC;
    if (src_here && src_fs.kind == FILE_KIND_DIR) return PUTDSTBAD;

    //  Dest parent dir must exist — no mkdir -p.  An empty dir slice
    //  means dest is at reporoot, which always exists.
    u8cs dst_dir = {};
    PATHu8sDir(dst_dir, dst);
    if (!u8csEmpty(dst_dir)) {
        a_path(dst_dir_fp);
        if (SNIFFFullpath(dst_dir_fp, reporoot, dst_dir) != OK)
            return PUTNODIR;
        filestat ddir_fs = {};
        if (FILELStat(&ddir_fs, $path(dst_dir_fp)) != OK ||
            ddir_fs.kind != FILE_KIND_DIR)
            return PUTNODIR;
    }

    //  Perform the rename only when src is still on disk.  Claim
    //  flow (dst-only) just records the row.
    if (src_here) {
        call(FILERename, $path(src_fp), $path(dst_fp));
    }

    //  Log row: path=src, fragment=final_dst.  Stamp the new file with
    //  the row's ts so the next bare `be put` fast-paths it.
    uri urow = {};
    urow.path[0]     = src_raw[0]; urow.path[1]     = src_raw[1];
    urow.fragment[0] = dst[0];     urow.fragment[1] = dst[1];
    call(SNIFFAtAppendAt, *ts_io, verb_put, &urow);
    (void)SNIFFAtStampPath(dst_fp, *ts_io);
    (*ts_io)++;
    (*emitted_io)++;
    done;
}

// --- Sub-add candidate detection (SUBS-009) -------------------------
//
//  `be put <subpath>` on a directory that is *meant to become a new
//  submodule* but is not yet mounted (no child `.be` anchor) can't be
//  staged as a file and isn't a parent dir either.  The plain
//  classifier would report a bare "does not exist", which is
//  misleading — the dir is right there.  Recognise the sub-add intent
//  so the caller can emit an actionable refusal that names the missing
//  mount step (`sniff sub-mount`) instead.
//
//  A path qualifies when ALL of:
//    * it is an on-disk directory under reporoot,
//    * it is NOT already a mount (`SNIFFSubIsMount` is NO — the
//      mounted case is handled by the gitlink-bump short-circuit), and
//    * it is named as a submodule by the wt's `.gitmodules`, OR it
//      already holds a child `.be` entry (a partially-set-up sub).
//
//  Pure read-only probe — opens `.gitmodules` RO if present, never
//  mutates.  `subpath` is the normalised, trailing-slash-stripped
//  rel path.  Returns YES on a sub-add candidate, NO otherwise.
static b8 put_is_sub_add_candidate(u8cs reporoot, u8cs subpath) {
    if (u8csEmpty(subpath)) return NO;
    if (SNIFFSubIsMount(reporoot, subpath)) return NO;

    //  Must be an on-disk directory.
    a_path(dirfp);
    if (SNIFFFullpath(dirfp, reporoot, subpath) != OK) return NO;
    filestat ds = {};
    if (FILELStat(&ds, $path(dirfp)) != OK ||
        ds.kind != FILE_KIND_DIR) return NO;

    //  (a) Child `.be` entry present — a partially-set-up sub.
    {
        a_path(bep);
        if (PATHu8bFeed(bep, reporoot) == OK &&
            PATHu8bAdd (bep, subpath) == OK) {
            a_cstr(be_s, ".be");
            if (PATHu8bPush(bep, be_s) == OK) {
                filestat bs = {};
                if (FILELStat(&bs, $path(bep)) == OK) return YES;
            }
        }
    }

    //  (b) Declared in the wt's `.gitmodules` (URL present).
    a_path(gmp);
    u8cs gmrel = {(u8c *)".gitmodules", (u8c *)".gitmodules" + 11};
    if (SNIFFFullpath(gmp, reporoot, gmrel) != OK) return NO;
    filestat gs = {};
    if (FILELStat(&gs, $path(gmp)) != OK ||
        gs.kind != FILE_KIND_REG) return NO;
    u8bp gm_map = NULL;
    if (FILEMapRO(&gm_map, $path(gmp)) != OK || gm_map == NULL) return NO;
    u8cs gm_blob = {u8bDataHead(gm_map), u8bIdleHead(gm_map)};
    a_pad(u8, url_buf, 2048);
    u8cs url = {};
    ok64 fr = SNIFFSubsParseFind(gm_blob, subpath, url_buf, url);
    FILEUnMap(gm_map);
    return fr == OK ? YES : NO;
}

// --- Public API ---

ok64 PUTStage(u32 nuris, uri const *uris) {
    sane(SNIFF.h && (nuris == 0 || uris != NULL));

    ron60 ts = 0;
    struct timespec tv = {};
    SNIFFAtNow(&ts, &tv);

    ron60 verb_put  = SNIFFAtVerbPut();
    ron60 verb_get  = SNIFFAtVerbGet();
    ron60 verb_post = SNIFFAtVerbPost();

    a_dup(u8c, reporoot, u8bData(SNIFF.h->wt));

    if (nuris == 0) {
        //  Walk the baseline tree — tracked-only; never picks up
        //  untracked siblings.  Submodules are skipped (WALKSKIP).
        sha1 tree_sha = {};
        ok64 bo = put_baseline_tree(&tree_sha);
        if (bo == ULOGNONE) {
            if (!SNIFF.quiet)
                fprintf(stderr,
                        "sniff: put: no baseline (fresh repo); name "
                        "files explicitly\n");
            return PUTNONE;
        }
        if (bo != OK) return bo;

        //  Pass 1: auto-pair system-`mv` renames.  Pairs are emitted
        //  as `put <old>#<new>` rows.  Refuses PUTAMBIG without
        //  writing anything when the sha pairing isn't 1:1.  Each
        //  emitted row bumps `ts` so subsequent rows stay strictly
        //  increasing.
        u32 mv_emitted = 0;
        ok64 mo = put_detect_moves(reporoot, &ts, &mv_emitted, verb_put);
        if (mo != OK) return mo;

        //  Pass 2: tracked-dirty walk.  Re-stamp ts: the latest
        //  get/post row's ts.  Files whose content matches the
        //  baseline get re-stamped to this so the next bare put
        //  fast-paths them via SNIFFAtKnown.  A path the move pass
        //  emitted a row for is still in the baseline tree but its
        //  on-disk file is absent — the put_visit_tracked NOENT
        //  branch returns OK silently, so no spurious dirty rows.
        ron60 base_ts = 0, base_verb = 0;
        uri base_u = {};
        (void)SNIFFAtBaseline(&base_ts, &base_verb, &base_u);

        put_walk_ctx wc = {.ts = ts, .verb_put = verb_put,
                           .baseline_ts = base_ts, .err = OK};
        u8csMv(wc.reporoot, reporoot);
        ok64 wo = WALKTreeLazy(tree_sha.data,
                               put_visit_tracked, &wc);
        if (wc.err != OK) return wc.err;
        if (wo != OK) return wo;
        u32 total = wc.emitted + mv_emitted;
        if (total == 0) {
            if (!SNIFF.quiet) fprintf(stderr, "sniff: put: no changes\n");
            return PUTNONE;
        }
        if (!SNIFF.quiet)
            fprintf(stderr, "sniff: staged %u put row(s)\n", total);
        done;
    }

    //  Per-path loop is driven by the unified ULOG-merge classifier
    //  (`SNIFFClassify`) — same primitive POST uses for its commit-
    //  time merge.  We pre-collect the user's requested paths, then
    //  let the merge tell us, per path, whether it's in baseline
    //  and/or on disk.  No path-set in memory, no per-path lstat —
    //  the wt cursor's `ts` field carries the file's mtime already.
    //
    //  Decisions:
    //    BOTH       + mtime ∈ get/post stamp → unchanged, skip
    //    BOTH       + otherwise              → stage (real edit)
    //    WT_ONLY    (on disk, not tracked)   → stage (new file)
    //    BASE_ONLY  (tracked, removed)       → skip ("does not exist")
    //    not seen   (typo / never existed)   → skip ("does not exist")

    //  Split file-form vs dir-form (path ends in `/`) requests.
    //  Dir-form rows are emitted as-is; POST expands them at commit
    //  time against the baseline tree.  File-form rows go through
    //  SNIFFClassify to validate path-in-baseline-or-on-disk.
    put_req reqs[CLI_MAX_URIS] = {};
    u32 nreq = 0;
    u32 emitted = 0;
    u32 skipped = 0;
    for (u32 i = 0; i < nuris; i++) {
        //  Move-form short-circuit: a non-empty fragment alongside a
        //  non-empty path is `be put <old>#<new>`.  Errors here refuse
        //  the whole command — moves are explicit user intent, not a
        //  best-effort sweep, so a bad arg should not slip through to
        //  the next loop iteration.
        if (!u8csEmpty(uris[i].fragment) && !u8csEmpty(uris[i].path)) {
            u8cs mvsrc = {uris[i].path[0],     uris[i].path[1]};
            u8cs mvdst = {uris[i].fragment[0], uris[i].fragment[1]};
            //  Strip leading "./" on both sides — mirrors the bareword
            //  normalisation a few lines down.
            if ($len(mvsrc) >= 2 && mvsrc[0][0] == '.' && mvsrc[0][1] == '/')
                mvsrc[0] += 2;
            if ($len(mvdst) >= 2 && mvdst[0][0] == '.' && mvdst[0][1] == '/')
                mvdst[0] += 2;
            ok64 mo = put_move(mvsrc, mvdst, reporoot,
                               &ts, &emitted, verb_put);
            if (mo != OK) return mo;
            continue;
        }

        u8cs raw = {};
        SNIFFAtPathBytes(&uris[i], raw);
        if (u8csEmpty(raw)) continue;
        //  Normalise reporoot references — `.`, `./`, leading `./` —
        //  so `be put .` (and `be put ./`) means "stage everything
        //  dirty/untracked under reporoot", same as `git add -A`.
        //  The dir-form path below treats an empty prefix as the
        //  reporoot directory.
        if ($len(raw) == 1 && raw[0][0] == '.') {
            raw[0] = raw[1];  // → empty = reporoot
        } else if ($len(raw) >= 2 && raw[0][0] == '.' && raw[0][1] == '/') {
            raw[0] += 2;
        }
        //  Refuse to stage sniff-meta paths (.be/wtlog / .be/* / .git*)
        //  even when explicitly named — they leak into legacy trees
        //  but must not propagate forward.
        if (!u8csEmpty(raw) && SNIFFSkipMeta(raw)) {
            fprintf(stderr,
                    "sniff: put: %.*s is a meta path — skipped\n",
                    (int)$len(raw), (char *)raw[0]);
            skipped++; continue;
        }

        //  Sub-mount short-circuit: `<wt>/<raw>/.be` is a regular file,
        //  so the path is a secondary worktree, not a parent directory.
        //  Read its pinned tip via the secondary-wt anchor and emit one
        //  `put <path>#<40-hex>` row.  No dir walk, no per-file hashing
        //  — the sub owns its own ULOG.  MODULES.plan.md §"Phase 4 —
        //  PUT".  Empty raw (`be put .`) falls through to the dir-form
        //  below; SubIsMount returns NO on empty subpath.
        {
            u8cs probe = {raw[0], raw[1]};
            if ($len(probe) > 0 && *(probe[1] - 1) == '/') probe[1]--;
            if (SNIFFSubIsMount(reporoot, probe)) {
                a_pad(u8, hexpad, 40);
                ok64 tr = SNIFFSubReadTip(reporoot, probe,
                                          u8bIdle(hexpad));
                if (tr != OK) {
                    fprintf(stderr,
                            "sniff: put: %.*s: no sub tip — skipped\n",
                            (int)$len(probe), (char *)probe[0]);
                    skipped++; continue;
                }
                //  u8sCopy is the consumed-slice form (doesn't advance
                //  the buffer's idle-head); commit the 40 written bytes
                //  to DATA explicitly so u8bDataC frames them.
                u8bFed(hexpad, 40);
                a_dup(u8c, hex_view, u8bDataC(hexpad));
                uri urow = {};
                urow.path[0]     = probe[0];   urow.path[1]     = probe[1];
                urow.fragment[0] = hex_view[0]; urow.fragment[1] = hex_view[1];
                call(SNIFFAtAppendAt, ts, verb_put, &urow);
                ts++;
                emitted++;
                continue;
            }
        }

        //  Sub-add candidate (SUBS-009): a dir named without a trailing
        //  slash that is declared in `.gitmodules` (or already holds a
        //  child `.be`) but is NOT yet mounted.  PUT can't stage it as a
        //  file, and synthesising a portable `.gitmodules` URL + picking
        //  a pin are open CLI-grammar decisions (the local anchor's
        //  row-0 is a `file:` shard pointer, not the upstream).  So emit
        //  an actionable refusal that names the missing mount step
        //  rather than the misleading "does not exist".  A trailing
        //  slash (`<sub>/`) is an explicit "stage the dir's contents"
        //  intent and falls through to the dir-form below unchanged.
        if (!u8csEmpty(raw) && !($len(raw) > 0 && *(raw[1] - 1) == '/')) {
            u8cs probe2 = {raw[0], raw[1]};
            if (put_is_sub_add_candidate(reporoot, probe2)) {
                fprintf(stderr,
                        "sniff: put: %.*s is an unmounted submodule — "
                        "mount it first with `sniff sub-mount "
                        "./%.*s#<pin>`, then `be put %.*s` stages the "
                        "gitlink\n",
                        (int)$len(probe2), (char *)probe2[0],
                        (int)$len(probe2), (char *)probe2[0],
                        (int)$len(probe2), (char *)probe2[0]);
                skipped++; continue;
            }
        }

        //  Empty raw (`.` or `./` after normalisation) is the
        //  reporoot dir form; trailing `/` is the explicit subdir
        //  dir form.  Both go through the dir-collect path.
        //
        //  A slashless arg that `stat`s as an on-disk directory is the
        //  SAME intent — `be put sub` must recurse exactly like
        //  `be put sub/` (DIS-034: the trailing slash regressed into a
        //  hard requirement, so `be put sub` reported "does not exist").
        //  Normalise it to the trailing-slash form here so the dir-form
        //  prefix below carries the `/` boundary (else the `u8csHasPrefix`
        //  expansion would over-match a sibling like `sub-other.txt`).
        //  A slashless arg that is NOT a directory (a file, a bareword
        //  branch word, a typo) stays file-form and is handled below.
        b8 is_dir = u8csEmpty(raw) ||
                    ($len(raw) > 0 && *(raw[1] - 1) == '/');
        //  Did the user type the dir WITHOUT a trailing slash?  We
        //  reframe it to `<dir>/` below (DIS-034), but remember the
        //  slashless intent so an empty/untracked expansion can suggest
        //  the explicit `<dir>/` form (SUBS-014) instead of a bare,
        //  misleading "unchanged".
        b8 reframed_slashless = NO;
        //  The slashless arg as the user typed it (before any reframe) —
        //  kept for the SUBS-014 hint subject.  `raw`'s bytes live in the
        //  URI text and outlive this reframe, so the slice stays valid.
        u8cs orig_raw = {raw[0], raw[1]};
        a_pad(u8, dir_buf, FILE_PATH_MAX_LEN);
        if (!is_dir) {
            a_path(probe_fp);
            filestat pfs = {};
            if (SNIFFFullpath(probe_fp, reporoot, raw) == OK &&
                FILELStat(&pfs, $path(probe_fp)) == OK &&
                pfs.kind == FILE_KIND_DIR) {
                //  Re-frame `raw` as `<dir>/` in a per-arg buffer so the
                //  dir-form prefix below carries the slash boundary.
                if (u8bFeed(dir_buf, raw) == OK &&
                    u8bFeed1(dir_buf, '/') == OK) {
                    a_dup(u8c, slashed, u8bDataC(dir_buf));
                    raw[0] = slashed[0];
                    raw[1] = slashed[1];
                    is_dir = YES;
                    reframed_slashless = YES;
                }
            }
        }
        if (is_dir) {
            //  Dir-form: confirm the dir exists, then expand into
            //  per-file `put` rows according to the tracked/untracked
            //  rule from https://replicated.wiki/html/wiki/PUT.html §PUT.  Empty expansion (no dirty
            //  files under a tracked dir, or empty wt subtree) is a
            //  no-op for this argument — the per-arg result mirrors
            //  the single-file `be put file.c` `is unchanged` skip.
            a_path(fp);
            if (SNIFFFullpath(fp, reporoot, raw) != OK) {
                fprintf(stderr,
                        "sniff: put: cannot resolve %.*s — skipped\n",
                        (int)$len(raw), (char *)raw[0]);
                skipped++; continue;
            }
            filestat fs = {};
            if (FILELStat(&fs, $path(fp)) != OK ||
                fs.kind != FILE_KIND_DIR) {
                fprintf(stderr,
                        "sniff: put: %.*s does not exist — skipped\n",
                        (int)$len(raw), (char *)raw[0]);
                skipped++; continue;
            }

            //  Stream tracked-and-dirty + untracked rows straight
            //  into the .be/wtlog ULOG from the merge callback.  Lex
            //  order is the merge's, which yields a deterministic
            //  row order under the prefix.
            dir_collect_ctx dctx = {
                .prefix      = {raw[0], raw[1]},
                .ts_io       = &ts,
                .emitted_io  = &emitted,
                .verb_put    = verb_put,
                .saw_tracked = NO,
                .err         = OK,
            };
            u8csMv(dctx.reporoot, reporoot);
            u32 before = emitted;
            ok64 cr = SNIFFClassify(dir_collect_step, &dctx);
            if (cr != OK) return cr;
            if (dctx.err != OK) return dctx.err;

            if (emitted == before) {
                if (dctx.saw_tracked) {
                    //  Tracked dir, nothing dirty under it — genuinely
                    //  unchanged.  Mirrors the single-file form.
                    fprintf(stderr,
                            "sniff: put: %.*s is unchanged — skipped\n",
                            (int)$len(raw), (char *)raw[0]);
                } else if (reframed_slashless) {
                    //  Untracked dir named WITHOUT a trailing slash that
                    //  expands to no stageable file (empty, or only
                    //  ignored/meta content).  Don't claim "unchanged" —
                    //  nothing was ever tracked here.  Subject is the arg
                    //  as typed (`orig_raw`); suggest the explicit
                    //  `<dir>/` form (`raw`, the reframed slice) so the
                    //  intent is unambiguous (SUBS-014).
                    fprintf(stderr,
                            "sniff: put: %.*s has no files to stage — "
                            "skipped (did you mean `%.*s`?)\n",
                            (int)$len(orig_raw), (char *)orig_raw[0],
                            (int)$len(raw),      (char *)raw[0]);
                } else {
                    //  Untracked dir named WITH a trailing slash, empty
                    //  expansion — no files to stage.
                    fprintf(stderr,
                            "sniff: put: %.*s has no files to stage — "
                            "skipped\n",
                            (int)$len(raw), (char *)raw[0]);
                }
                skipped++;
            }
            continue;
        }

        if (nreq < CLI_MAX_URIS) {
            reqs[nreq].raw[0] = raw[0];
            reqs[nreq].raw[1] = raw[1];
            nreq++;
        }
    }

    if (nreq > 0) {
        put_ctx pctx = {.reqs = reqs, .n = nreq,
                        .verb_get = verb_get, .verb_post = verb_post};
        call(SNIFFClassify, put_classify_step, &pctx);

        for (u32 i = 0; i < nreq; i++) {
            put_req *r = &reqs[i];
            if (!r->stage) {
                //  `does not exist` is only accurate when the path is
                //  truly absent on disk.  An unseen-but-present path
                //  (anything the classifier couldn't account for —
                //  directories now recurse above, so this is the
                //  residual oddball: a special file, a path the merge
                //  skipped) gets an accurate "exists but is not
                //  stageable" rather than the misleading "does not
                //  exist" (DIS-034).
                char const *reason;
                if (r->seen) {
                    reason = r->skip_reason;
                } else {
                    a_path(pfp);
                    filestat pfs = {};
                    if (SNIFFFullpath(pfp, reporoot, r->raw) == OK &&
                        FILELStat(&pfs, $path(pfp)) == OK) {
                        reason = "exists but is not stageable";
                    } else {
                        reason = "does not exist";
                    }
                }
                fprintf(stderr, "sniff: put: %.*s %s — skipped\n",
                        (int)$len(r->raw), (char *)r->raw[0], reason);
                skipped++;
                continue;
            }
            a_path(fp);
            if (SNIFFFullpath(fp, reporoot, r->raw) != OK) {
                fprintf(stderr,
                        "sniff: put: cannot resolve %.*s — skipped\n",
                        (int)$len(r->raw), (char *)r->raw[0]);
                skipped++;
                continue;
            }
            uri urow = {};
            urow.path[0] = r->raw[0];
            urow.path[1] = r->raw[1];
            call(SNIFFAtAppendAt, ts, verb_put, &urow);
            (void)SNIFFAtStampPath(fp, ts);
            ts++;
            emitted++;
        }
    }

    if (emitted == 0) {
        if (skipped > 0)
            fprintf(stderr, "sniff: put: no eligible paths\n");
        return PUTNONE;
    }
    fprintf(stderr, "sniff: staged %u put row(s)\n", emitted);
    done;
}

// --- Branch-form PUT helpers ----------------------------------------
//
//  `be put ?<branch>`        → PUTCreateBranch (label-only fork at cur.tip)
//  `be put ?<branch>#<sha>`  → PUTSetBranch    (label move + cross-shard
//                                                migration)
//  `be put ?<refkey>` with explicit sha (legacy PUTSetLabel path) →
//                              PUTSetLabel (canonicalise key, append REFS)
//
//  These are PUT-side ref-writers — they never create commits.  Naming
//  matches https://replicated.wiki/html/wiki/POST.html's verb semantics (POST = commit-maker, PUT =
//  ref-writer); historical residence in POST.c was an accumulation
//  artefact.  See git history for the move.

ok64 PUTCreateBranch(u8cs reporoot, u8cs target_branch) {
    sane($ok(reporoot) && $ok(target_branch));
    sha1 existing = {};
    ok64 er = POSTResolveBranchTip(&existing, target_branch);
    if (er == OK) {
        fprintf(stderr,
                "sniff: put: ?" U8SFMT " already exists\n",
                u8sFmt(target_branch));
        return PUTDUP;
    }
    if (er != REFSNONE) return er;
    //  Create-only: POSTPromote with allow_create=YES handles the
    //  create-on-miss arm, but doesn't auto-sync cur (per spec).
    //  Pass "put" so its log lines name the verb the user typed.
    (void)reporoot;
    a_cstr(put_tag, "put");
    return POSTPromote(target_branch, YES, put_tag);
}

ok64 PUTSetBranch(u8cs reporoot, u8cs target_branch, u8cs sha_hex) {
    sane($ok(reporoot) && $ok(target_branch) && $ok(sha_hex));
    if (u8csLen(sha_hex) != 40 || !HEXu8sValid(sha_hex)) fail(SNIFFFAIL);

    keeper *k = &KEEP;

    //  Resolve cur's branch — empty cur_branch means trunk.  Used only
    //  by the same-shard short-circuit below (PUT permits re-pointing
    //  cur's own ref directly).  Matches the lookup POSTPromote does at
    //  its head.
    a_pad(u8, cur_buf, 256);
    {
        ron60 ts = 0, verb = 0;
        uri u = {};
        if (SNIFFAtCurTip(&ts, &verb, &u) == OK) {
            u8cs branch = {};
            DOGQueryBranchOnly(u.query, branch);
            if (!u8csEmpty(branch)) u8bFeed(cur_buf, branch);
        }
    }
    a_dup(u8c, cur_branch, u8bData(cur_buf));

    a_path(keepdir);
    call(HOMEBranchDir, KEEP.h, keepdir, NULL);

    a_pad(u8, keybuf, 256);
    u8bFeed1(keybuf, '?');
    u8bFeed(keybuf, target_branch);
    a_dup(u8c, refkey, u8bData(keybuf));

    a_pad(u8, valbuf, 64);
    u8bFeed1(valbuf, '?');
    u8bFeed(valbuf, sha_hex);
    a_dup(u8c, val, u8bData(valbuf));

    //  Same-shard short-circuit: target == cur means the new tip's
    //  objects already live in the active leaf — no migration, no
    //  switch.  PUT semantics permit re-pointing cur's own ref.
    if (u8csEq(target_branch, cur_branch))
        return REFSAppendVerb($path(keepdir), REFSVerbPost(), refkey, val);

    //  PUT is UNCONSTRAINED (https://replicated.wiki/html/wiki/PUT.html
    //  §PUT; https://replicated.wiki/html/wiki/Invariants.html §"POST is
    //  fast-forward only, PUT unconstrained"): a `?branch#<sha>` tip-set
    //  writes ANY ref to ANY sha — non-FF, no reachability check.  POST
    //  is the FF-advance verb; PUT is the reflog escape hatch (roll a
    //  contaminated `?main` back to a known-good commit).  So there is
    //  NO shared-ancestry / FP-chain (POST_MIG) walk here.  That walk
    //  used to refuse a non-descendant sha (`SNIFFFAIL "no shared
    //  ancestry"`) and silently ignored `--force`/`!`, contradicting the
    //  spec (PUT-002).
    //
    //  Under the flat store every object already lives in the one
    //  project pool, so the set is a pure REFS append regardless of
    //  whether the new tip FP-reaches the old tip — there is never a
    //  cross-shard migration to validate.  The sha was already resolved
    //  to a real object in the pool by the dispatcher (KEEPResolveHex /
    //  KEEPResolveRef in sniff/SNIFF.exe.c), so a typo surfaces there,
    //  not here.
    //
    //  Materialise the target leaf's shard dir for a NOT-yet-existing
    //  branch (idempotent on KEEPDUP / KEEPTRUNK) so the new ref is a
    //  valid leaf; an existing target needs no dir work.
    {
        sha1 target_tip = {};
        ok64 tr = POSTResolveBranchTip(&target_tip, target_branch);
        if (tr != OK && tr != REFSNONE) return tr;
        if (tr == REFSNONE) {
            ok64 ko = KEEPCreateBranch(k->h, target_branch);
            if (ko != OK && ko != KEEPDUP && ko != KEEPTRUNK) return ko;
        }
    }

    return REFSAppendVerb($path(keepdir), REFSVerbPost(), refkey, val);
}

ok64 PUTSetLabel(u8cs ref_uri, u8cs sha_hex) {
    sane($ok(ref_uri) && !u8csEmpty(ref_uri) && $ok(sha_hex));
    if (u8csLen(sha_hex) != 40) fail(SNIFFFAIL);

    a_path(keepdir);
    call(HOMEBranchDir, KEEP.h, keepdir, NULL);

    //  Canonicalise the caller-supplied ref URI (user input path:
    //  command line `be post ?<label>`).  Lex → canonicalise → feed.
    uri u = {};
    u.data[0] = ref_uri[0];
    u.data[1] = ref_uri[1];
    call(URILexer, &u);
    u.data[0] = ref_uri[0];
    u.data[1] = ref_uri[1];
    a_pad(u8, keybuf, 256);
    call(DOGCanonURIFeed, keybuf, &u);
    a_dup(u8c, key, u8bData(keybuf));

    //  Materialise the per-branch keeper shard for non-trunk labels
    //  before recording the REFS row.  KEEPCreateBranch normalises the
    //  branch (empty = trunk, mapped trunk aliases collapse to trunk
    //  too) and is idempotent on KEEPDUP — letting two POSTs against
    //  the same fresh label converge without a separate "branch
    //  exists" probe.  KEEPTRUNK is silently absorbed (trunk shard
    //  always exists by construction).
    if (!$empty(u.query)) {
        a_dup(u8c, branch, u.query);
        ok64 ko = KEEPCreateBranch(KEEP.h, branch);
        if (ko != OK && ko != KEEPDUP && ko != KEEPTRUNK) return ko;
    }

    //  Val is bare 40-hex (canonical).  `post` verb — local ref move.
    return REFSAppendVerb($path(keepdir), REFSVerbPost(), key, sha_hex);
}
