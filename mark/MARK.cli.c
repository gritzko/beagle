//  mark CLI — render StrictMark to standalone HTML.
//
//    mark [--strict] <file.mkd | dir>...
//
//  A file → file.html next to it.  A directory → every *.mkd in it,
//  rewriting inter-page links from .mkd to .html.  --strict makes a
//  WikiWeb structure/limit violation a hard failure.

#include "MARK.h"

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"

#include <string.h>

//  Strip a trailing ".mkd" from a slice (term retreats); else unchanged.
static void mark_dropext(u8csp s) {
    if (u8csLen(s) >= 4) {
        a_tail(u8c, suf, s, 4);
        if (memcmp(suf[0], ".mkd", 4) == 0) s[1] = suf[0];
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

//  Read inpath, render to opath.  `out` is the (reset) HTML buffer.
static ok64 mark_render_inner(u8bp out, path8s inpath, path8b opath,
                              markopts opts) {
    sane($ok(inpath) && out != NULL);

    u8bp m = NULL;
    call(FILEMapRO, &m, inpath);
    if (m == NULL) fail(MARKFAIL);
    u8cs src = {u8bDataHead(m), u8bIdleHead(m)};

    u8cs title = {};
    PATHu8sBase(title, inpath);
    mark_dropext(title);

    call(mark_outpath, opath, inpath);
    u8bReset(out);
    try(MARKRenderDoc, out, src, title, opts);
    ok64 ro = __;
    FILEUnMap(m);
    if (ro != OK) return ro;

    int fd = -1;
    FILEUnLink($path(opath));
    call(FILECreate, &fd, $path(opath));
    u8cs body = {u8bDataHead(out), u8bIdleHead(out)};
    try(FILEFeedAll, fd, body);
    ro = __;
    FILEClose(&fd);
    fprintf(stderr, "mark: wrote %s\n", (char const *)*$path(opath));
    return ro;
}

typedef struct {
    markopts opts;
    u8bp out;
} mark_dir_ctx;

//  FILEScanFiles callback: render each *.mkd in the directory.
static ok64 mark_dir_cb(void0p arg, path8bp path) {
    sane(arg != NULL && path != NULL);
    mark_dir_ctx *dc = (mark_dir_ctx *)arg;
    u8cs p = {u8bDataHead(path), u8bIdleHead(path)};
    if (u8csLen(p) < 4) done;
    a_tail(u8c, suf, p, 4);
    if (memcmp(suf[0], ".mkd", 4) != 0) done;
    a_path(opath);
    //  Don't let one bad page abort the whole batch; warn and continue.
    try(mark_render_inner, dc->out, p, opath, dc->opts);
    nedo fprintf(stderr, "mark: %s failed: %s\n", (char const *)*p, ok64str(__));
    return OK;
}

static ok64 markcli_inner(markopts opts, u8bp out) {
    sane(out != NULL);
    b8 any = NO;
    for (size_t i = 1; i < (size_t)$arglen; ++i) {
        a$rg(arg, i);
        if (u8csEmpty(arg) || u8csAt(arg, 0) == '-') continue;  // skip flags
        any = YES;
        a_path(inpath, arg);
        filestat fs = {};
        call(FILEStat, &fs, $path(inpath));
        if (fs.kind == FILE_KIND_DIR) {
            mark_dir_ctx dc = {.opts = opts, .out = out};
            call(FILEScanFiles, inpath, mark_dir_cb, &dc);
        } else {
            a_path(opath);
            call(mark_render_inner, out, $path(inpath), opath, opts);
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
    for (size_t i = 1; i < (size_t)$arglen; ++i) {
        a$rg(a, i);
        a_cstr(strict, "--strict");
        if (u8csLen(a) == u8csLen(strict) &&
            memcmp(a[0], strict[0], u8csLen(strict)) == 0)
            opts.strict = YES;
    }

    u8b out = {};
    call(u8bAllocate, out, 1UL << 20);
    try(markcli_inner, opts, out);
    ok64 ret = __;
    u8bFree(out);
    return ret;
}

MAIN(markcli);
