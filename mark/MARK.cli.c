//  mark CLI — render StrictMark to standalone HTML.
//
//    mark [--strict] [--head=FILE] [--body=FILE] <file.mkd | dir>...
//
//  A file → file.html next to it.  A directory → every *.mkd in it,
//  rewriting inter-page links from .mkd to .html.  --strict makes a
//  WikiWeb structure/limit violation a hard failure.  --head=FILE inlines
//  that file's contents verbatim into every page's <head> (stylesheet,
//  favicon, meta...); --body=FILE inlines a snippet right after <body>
//  (a banner, nav...).  So mark bakes in no site policy; each file is
//  mapped once and shared across the whole render.
//
//  Memory is bounded by the input: the renderer keeps nothing across
//  files, and the per-file output buffer is sized to that file (escaping
//  expands at most ~6x), so there is no fixed multi-megabyte allocation.

#include "MARK.h"

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"

#include <string.h>

//  Strip a trailing ".mkd" from a slice (term retreats); else unchanged.
static void mark_dropext(u8csp s) {
    a_cstr(mkd, ".mkd");
    if (u8csHasSuffix(s, mkd)) {
        a_tail(u8c, suf, s, u8csLen(mkd));
        s[1] = suf[0];
    }
}

//  outpath := inpath with ".mkd" replaced by ".html".
static ok64 mark_outpath(path8b out, u8csc in) {
    sane($ok(in) && out != NULL);
    u8cs stem = {in[0], in[1]};
    mark_dropext(stem);
    call(PATHu8bDup, out, stem);
    a_cstr(dot, ".html");
    call(PATHu8bFeed, out, dot);
    done;
}

//  Map inpath, render to opath.  The output buffer is allocated here,
//  sized to the mapped input, and freed before returning — so memory is
//  proportional to one file and nothing is retained between files.
static ok64 mark_render_inner(path8s inpath, path8b opath, markopts opts) {
    sane($ok(inpath));

    u8bp m = NULL;
    call(FILEMapRO, &m, inpath);
    if (m == NULL) fail(MARKFAIL);
    u8cs src = {u8bDataHead(m), u8bIdleHead(m)};

    u8b out = {};
    size_t cap = (size_t)u8csLen(src) * 8 + 16384;
    try(u8bAllocate, out, cap);
    if (__ != OK) {
        FILEUnMap(m);
        return __;
    }

    u8cs title = {};
    PATHu8sBase(title, inpath);
    mark_dropext(title);

    try(mark_outpath, opath, inpath);
    then try(MARKRenderDoc, out, src, title, opts);
    then {
        int fd = -1;
        FILEUnLink($path(opath));
        try(FILECreate, &fd, $path(opath));
        then {
            u8cs body = {u8bDataHead(out), u8bIdleHead(out)};
            try(FILEFeedAll, fd, body);
            FILEClose(&fd);
        }
        then fprintf(stderr, "mark: wrote %s\n", (char const *)*$path(opath));
    }
    ok64 ro = __;

    u8bFree(out);
    FILEUnMap(m);
    return ro;
}

typedef struct {
    markopts opts;
    ok64     breach;  // first non-OK render across the walk (CI gate)
} mark_dir_ctx;

//  FILEScanFiles callback: render each *.mkd in the directory.  A bad
//  page warns and the batch continues so every breach is reported; the
//  first non-OK result is remembered in the ctx and propagated by the
//  caller, so a directory build still fails (non-zero exit) under
//  --strict.  Returning OK here is deliberate — a non-OK return would
//  abort the scan and hide the remaining breaches.
static ok64 mark_dir_cb(void0p arg, path8bp path) {
    sane(arg != NULL && path != NULL);
    mark_dir_ctx *dc = (mark_dir_ctx *)arg;
    u8cs p = {u8bDataHead(path), u8bIdleHead(path)};
    a_cstr(mkd, ".mkd");
    if (!u8csHasSuffix(p, mkd)) done;
    a_path(opath);
    try(mark_render_inner, p, opath, dc->opts);
    nedo {
        fprintf(stderr, "mark: %s failed: %s\n", (char const *)*p,
                ok64str(__));
        if (dc->breach == OK) dc->breach = __;
    }
    return OK;
}

static ok64 markcli_inner(markopts opts) {
    sane(1);
    b8 any = NO;
    for (size_t i = 1; i < (size_t)$arglen; ++i) {
        a$rg(arg, i);
        if (u8csEmpty(arg) || u8csAt(arg, 0) == '-') continue;  // skip flags
        any = YES;
        a_path(inpath, arg);
        filestat fs = {};
        call(FILEStat, &fs, $path(inpath));
        if (fs.kind == FILE_KIND_DIR) {
            mark_dir_ctx dc = {.opts = opts, .breach = OK};
            call(FILEScanFiles, inpath, mark_dir_cb, &dc);
            if (dc.breach != OK) return dc.breach;  // CI gate: fail the build
        } else {
            a_path(opath);
            call(mark_render_inner, $path(inpath), opath, opts);
        }
    }
    if (!any) {
        fprintf(stderr, "usage: mark [--strict] <file.mkd | dir>...\n");
        return MARKARG;
    }
    done;
}

ok64 markcli() {
    sane(1);
    call(FILEInit);

    markopts opts = {};
    a_path(headpath);
    a_path(bodypath);
    b8 want_head = NO, want_body = NO;
    for (size_t i = 1; i < (size_t)$arglen; ++i) {
        a$rg(a, i);
        a_cstr(strict, "--strict");
        if (u8csEq(a, strict)) {
            opts.strict = YES;
            continue;
        }
        a_cstr(headpfx, "--head=");
        if (u8csLen(a) > u8csLen(headpfx) && u8csHasPrefix(a, headpfx)) {
            a_rest(u8c, val, a, u8csLen(headpfx));
            call(PATHu8bDup, headpath, val);
            want_head = YES;
        }
        a_cstr(bodypfx, "--body=");
        if (u8csLen(a) > u8csLen(bodypfx) && u8csHasPrefix(a, bodypfx)) {
            a_rest(u8c, val, a, u8csLen(bodypfx));
            call(PATHu8bDup, bodypath, val);
            want_body = YES;
        }
    }

    //  Map the head/body snippets once (resources at the top of the chain);
    //  the slices stay valid for the whole render and are unmapped after.
    u8bp hm = NULL;
    if (want_head) {
        call(FILEMapRO, &hm, $path(headpath));
        if (hm == NULL) return MARKARG;
        opts.head[0] = u8bDataHead(hm);
        opts.head[1] = u8bIdleHead(hm);
    }
    u8bp bm = NULL;
    if (want_body) {
        call(FILEMapRO, &bm, $path(bodypath));
        if (bm == NULL) {
            if (hm != NULL) FILEUnMap(hm);
            return MARKARG;
        }
        opts.body[0] = u8bDataHead(bm);
        opts.body[1] = u8bIdleHead(bm);
    }

    try(markcli_inner, opts);
    ok64 ro = __;
    if (hm != NULL) FILEUnMap(hm);
    if (bm != NULL) FILEUnMap(bm);
    return ro;
}

MAIN(markcli);
