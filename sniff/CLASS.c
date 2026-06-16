//  CLASS — see CLASS.h.

#include "CLASS.h"

#include <string.h>

#include "abc/B.h"
#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PRO.h"
#include "abc/RON.h"
#include "abc/URI.h"
#include "dog/DOG.h"
#include "dog/git/GIT.h"
#include "keeper/KEEP.h"
#include "keeper/WALK.h"

#include "AT.h"
#include "SNIFF.h"

#define CLASS_PD_BUF (1UL << 20)

//  Baseline tree + wt scan can grow into the tens of thousands of
//  ULOG rows for real-world wts (full repo at /home/gritzko/dogs has
//  ~700 tracked paths after gitignore, plus the baseline tree's
//  per-path entries).  4 MB each, mmap-backed (lazy paging).
#define CLASS_BU_BUF (1UL << 22)
#define CLASS_WU_BUF (1UL << 22)

// --- Resolve baseline tree sha ---

static ok64 class_baseline_tree(sha1 *out, b8 *have_out) {
    sane(out && have_out);
    ok64 br = SNIFFAtBaselineTreeSha(YES, out, have_out);
    if (br == ULOGNONE) { *have_out = NO; done; }    // fresh repo
    return br;
}

// --- Staged put/delete row collector ---
//
//  Walks `.be/wtlog` rows since the last post via `SNIFFAtScanPutDelete`,
//  emitting per-row ULOG bytes into the matching unsorted buffer.
//  Sort is the heap-merge's job; we just produce two cursors.

#define CLASS_SUB_BUF (1UL << 12)

typedef struct {
    u8bp  put_buf;
    u8bp  del_buf;
    ron60 v_put_filter;
    ron60 v_del_filter;
    ron60 v_put_emit;
    ron60 v_del_emit;
    ok64  err;
} class_pd_ctx;

static ok64 class_pd_cb(ulogreccp src, void *ctx_) {
    class_pd_ctx *c = (class_pd_ctx *)ctx_;
    u8cs path = {src->uri.path[0], src->uri.path[1]};
    //  Skip dir-prefix rows (`put lib/` etc.) — for status we accept
    //  the imprecision; expanding against bu/wu would replicate POST.
    if (!$empty(path) && *u8csLast(path) == '/') return OK;
    //  Preserve the full URI (path + fragment) so move-form put rows
    //  round-trip into the merge — status display reads .fragment to
    //  pair source/dest into one `mov` row.
    if (src->verb == c->v_put_filter) {
        ulogrec rec = {.ts = src->ts, .verb = c->v_put_emit,
                       .uri = src->uri};
        ok64 r = ULOGu8sFeed(u8bIdle(c->put_buf), &rec);
        if (r != OK) c->err = r;
        return r;
    }
    if (src->verb == c->v_del_filter) {
        ulogrec rec = {.ts = src->ts, .verb = c->v_del_emit,
                       .uri = src->uri};
        ok64 r = ULOGu8sFeed(u8bIdle(c->del_buf), &rec);
        if (r != OK) c->err = r;
        return r;
    }
    return OK;
}

//  Sort `src` (per-row ULOG bytes) into `dst` lex-by-path.  Heap-pop
//  pattern straight from POST.c's post_sort_dedup_intent.
static ok64 class_sort_pd(u8b src, u8b dst) {
    sane(src && dst);
    u8bReset(dst);
    if (u8bDataLen(src) == 0) done;
    Bu8cs slices = {};
    size_t cap = u8bDataLen(src) / 16 + 16;
    call(u8csbAllocate, slices, cap);
    a_dup(u8c, scan, u8bDataC(src));
    while (!u8csEmpty(scan)) {
        a_dup(u8c, find, scan);
        //  Line includes its '\n' (if any): advance `find` past the
        //  newline, then take the prefix of `scan` up to that point.
        if (u8csFind(find, '\n') == OK) u8csUsed1(find);
        a_past(u8c, slice, scan, find);
        ok64 fo = u8csbFeedP(slices, &slice);
        if (fo != OK) { u8csbFree(slices); return fo; }
        u8csMv(scan, find);
    }
    u8cssHeapZ(u8csbData(slices), ULOGu8csZbyUri);
    while (u8csbDataLen(slices) > 0) {
        u8cs cur = {};
        ok64 po = HEAPu8csPopZ(&cur, slices, ULOGu8csZbyUri);
        if (po != OK) break;
        a_dup(u8c, cur_dup, cur);
        ok64 fo = u8bFeed(dst, cur_dup);
        if (fo != OK) { u8csbFree(slices); return fo; }
    }
    u8csbFree(slices);
    done;
}

// --- SNIFFMergeWalk step shim ---
//
//  Tracks an active set of submodule (`160000` gitlink) directory
//  prefixes seen earlier in the merge.  Any subsequent step whose
//  path starts with `<prefix>/` is silently dropped — gitlinks own
//  their internal state.  The merge yields paths in lex order, so a
//  submodule's own row arrives before any of its descendants.

typedef struct {
    class_cb cb;
    void    *ctx;
    ron60    v_base;
    ron60    v_wt;
    ron60    v_put;
    ron60    v_del;
    Bu8      sub_prefixes;   // newline-separated `<path>/`
    b8       sub_init;
} class_walk_ctx;

static b8 class_under_submodule(class_walk_ctx const *w, u8cs path) {
    if (!w->sub_init) return NO;
    a_dup(u8c, scan, u8bDataC(w->sub_prefixes));
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

static ok64 class_remember_submodule(class_walk_ctx *w, u8cs path) {
    //  Buffer is pre-carved by SNIFFClassify; sub_init flags "≥1 sub
    //  recorded" so class_under_submodule knows whether to scan.
    w->sub_init = YES;
    (void)u8bFeed(w->sub_prefixes, path);
    (void)u8bFeed1(w->sub_prefixes, '/');
    (void)u8bFeed1(w->sub_prefixes, '\n');
    return OK;
}

static ok64 class_merge_step(ulogreccs recs, void *ctx_) {
    class_walk_ctx *w = (class_walk_ctx *)ctx_;

    ulogreccp base = NULL, wt = NULL, put = NULL, del = NULL;
    $for(ulogrec const, rec, recs) {
        if      (ok64stem(rec->verb) == w->v_base) base = rec;
        else if (ok64stem(rec->verb) == w->v_wt)   wt   = rec;
        else if (rec->verb == w->v_put)            put  = rec;
        else if (rec->verb == w->v_del)            del  = rec;
    }

    u8cs path = {};
    ulogreccp src = base ? base : wt ? wt : put ? put : del;
    if (!src) return OK;
    path[0] = src->uri.path[0];
    path[1] = src->uri.path[1];

    //  Anything inside a previously-recorded submodule prefix → drop.
    //  Catches wt-scan rows that descended into the gitlinked dir
    //  (the embedded repo's own files have no business in our status).
    if (class_under_submodule(w, path)) return OK;

    //  Gitlink rows themselves (kind `s` in baseline verb) → record
    //  the path as a prefix so the sub's internals are filtered
    //  (`class_under_submodule` above), then surface the mount itself
    //  as one step so callers (bare-`be` status, POST) see staged
    //  bumps (`put <sub>#<40-hex>`) and the current pin.
    if (base != NULL && ok64Lit(base->verb, 0) == RON_s) {
        (void)class_remember_submodule(w, path);
        class_step step = {};
        step.kind = (wt != NULL) ? CLASS_BOTH : CLASS_BASE_ONLY;
        step.path[0] = path[0]; step.path[1] = path[1];
        step.base_rec = base;
        step.wt_rec   = wt;
        step.put_rec  = put;
        step.del_rec  = del;
        return w->cb(&step, w->ctx);
    }

    class_step step = {};
    if (base != NULL && wt != NULL)        step.kind = CLASS_BOTH;
    else if (base != NULL)                 step.kind = CLASS_BASE_ONLY;
    else                                   step.kind = CLASS_WT_ONLY;
    step.path[0] = path[0];
    step.path[1] = path[1];
    step.base_rec = base;
    step.wt_rec   = wt;
    step.put_rec  = put;
    step.del_rec  = del;
    return w->cb(&step, w->ctx);
}

// --- Public entry ---

ok64 SNIFFClassify(class_cb cb, void *ctx) {
    sane(cb);
    a_dup(u8c, reporoot, u8bData(HOME.wt));

    a_cstr(base_name, "base");
    a_cstr(wt_name,   "wt");
    a_cstr(put_name,  "put");
    a_cstr(del_name,  "del");
    ron60 v_base = SNIFFAtVerbOf(base_name);
    ron60 v_wt   = SNIFFAtVerbOf(wt_name);
    ron60 v_put  = SNIFFAtVerbOf(put_name);
    ron60 v_del  = SNIFFAtVerbOf(del_name);

    //  Merge-input scratch, all BASS-carved (rewinds when SNIFFClassify
    //  returns): ~12 MB total, sized once per status/ls/prune run.
    a_carve(u8, bu, CLASS_BU_BUF);
    a_carve(u8, wu, CLASS_WU_BUF);
    a_carve(u8, pu_unsorted, CLASS_PD_BUF);
    a_carve(u8, du_unsorted, CLASS_PD_BUF);
    a_carve(u8, pu, CLASS_PD_BUF);
    a_carve(u8, du, CLASS_PD_BUF);

    sha1 base_tree = {};
    b8 have_base = NO;
    call(class_baseline_tree, &base_tree, &have_base);
    if (have_base) call(KEEPTreeULog, base_tree.data, 0, v_base, bu);
    call(SNIFFWtULog, reporoot, v_wt, wu);

    //  Pull every put/delete row since the most recent post into the
    //  unsorted intent buffers, then sort each by URI key.
    class_pd_ctx pdc = {
        .put_buf = pu_unsorted, .del_buf = du_unsorted,
        .v_put_filter = SNIFFAtVerbPut(),
        .v_del_filter = SNIFFAtVerbDelete(),
        .v_put_emit   = v_put,
        .v_del_emit   = v_del,
    };
    ron60 floor = SNIFFAtLastPostTs();
    call(SNIFFAtScanPutDelete, floor, class_pd_cb, &pdc);
    if (pdc.err != OK) return pdc.err;
    call(class_sort_pd, pu_unsorted, pu);
    call(class_sort_pd, du_unsorted, du);

    a_dup(u8c, view_b, u8bData(bu));
    a_dup(u8c, view_w, u8bData(wu));
    a_dup(u8c, view_p, u8bData(pu));
    a_dup(u8c, view_d, u8bData(du));
    a_pad(u8cs, ins, 4);
    u8cssFeed1(ins_idle, view_b);
    u8cssFeed1(ins_idle, view_w);
    u8cssFeed1(ins_idle, view_p);
    u8cssFeed1(ins_idle, view_d);
    a_dup(u8cs, cursors, u8csbData(ins));

    //  Submodule-prefix accumulator (filled lazily by
    //  class_remember_submodule during the walk); BASS-carved here so
    //  it outlives the bare per-step callback and rewinds on return.
    a_carve(u8, sub_prefixes, CLASS_SUB_BUF);
    class_walk_ctx wctx = {.cb = cb, .ctx = ctx,
                           .v_base = v_base, .v_wt = v_wt,
                           .v_put = v_put,   .v_del = v_del};
    ((u8 **)wctx.sub_prefixes)[0] = sub_prefixes[0];
    ((u8 **)wctx.sub_prefixes)[1] = sub_prefixes[1];
    ((u8 **)wctx.sub_prefixes)[2] = sub_prefixes[2];
    ((u8 **)wctx.sub_prefixes)[3] = sub_prefixes[3];
    call(SNIFFMergeWalk, cursors, class_merge_step, &wctx);
    done;
}

// --- Touched-unchanged content check -----------------------------------

b8 CLASSWtEqBase(u8cs reporoot, ulogreccp base_rec, u8cs rel) {
    if (!base_rec || u8csLen(base_rec->uri.fragment) != 40) return NO;
    a_path(fp);
    if (SNIFFFullpath(fp, reporoot, rel) != OK) return NO;
    filestat fs = {};
    if (FILELStat(&fs, $path(fp)) != OK) return NO;

    sha1 wt_sha = {};
    if (fs.kind == FILE_KIND_LNK) {
        a_pad(u8, tgt, 4096);
        if (FILEReadLink(tgt, $path(fp)) != OK) return NO;
        KEEPObjSha(&wt_sha, DOG_OBJ_BLOB, u8bDataC(tgt));
    } else if (fs.kind == FILE_KIND_REG) {
        if (fs.size == 0) {
            u8cs empty = {NULL, NULL};
            KEEPObjSha(&wt_sha, DOG_OBJ_BLOB, empty);
        } else {
            u8bp m = NULL;
            if (FILEMapRO(&m, $path(fp)) != OK) return NO;
            u8cs body = {u8bDataHead(m), u8bIdleHead(m)};
            KEEPObjSha(&wt_sha, DOG_OBJ_BLOB, body);
            FILEUnMap(m);
        }
    } else {
        return NO;
    }

    sha1 base_sha = {};
    if (sha1FromHex(&base_sha, base_rec->uri.fragment) != OK) return NO;
    return sha1Eq(&wt_sha, &base_sha);
}

// --- CLASS_BOTH verdict (DIS-023) --------------------------------------

//  YES iff `mtime` stamps a `patch` row — the file's current bytes are
//  the merged result of an in-scope PATCH absorption, staged for the
//  next POST but not yet committed.
static b8 class_mtime_is_patch(ron60 mtime) {
    if (mtime == 0 || !SNIFFAtKnown(mtime)) return NO;
    ron60 verb = 0;
    uri u = {};
    if (SNIFFAtRowAtTs(mtime, &verb, &u) != OK) return NO;
    return verb == SNIFFAtVerbPatch() ? YES : NO;
}

class_wt_state CLASSWtState(u8cs reporoot, class_step const *step) {
    if (step->wt_rec == NULL) return CLASS_WT_MODIFIED;
    ron60 mtime = step->wt_rec->ts;
    //  Patched-in bytes surface as their own state regardless of the
    //  baseline hash (the merge result is intentionally != baseline).
    if (class_mtime_is_patch(mtime)) return CLASS_WT_PATCHED;
    //  CLEAN only when the on-disk bytes actually hash to the baseline
    //  blob.  A known-stamp mtime is just a hint: confirm by content so
    //  a restored-stamp mtime over edited bytes can't read as clean
    //  (the non-idempotent-status bug, DIS-023).  A drifted mtime whose
    //  bytes still equal baseline is the "touched-unchanged" case and
    //  is likewise CLEAN.
    u8cs rel = {step->path[0], step->path[1]};
    if (CLASSWtEqBase(reporoot, step->base_rec, rel)) return CLASS_WT_CLEAN;
    return CLASS_WT_MODIFIED;
}
