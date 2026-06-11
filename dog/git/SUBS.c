//  SUBS — git `.gitmodules` parser + synthesizer.  See SUBS.h.
//
//  Implementation lifted from `sniff/SUBS.c` so keeper can use it
//  without depending on sniff.  The sniff-side names
//  (`SNIFFSubsParse`, …) remain as one-line wrappers in
//  `sniff/SUBS.c` to keep existing call sites compiling.

#include "SUBS.h"

#include "abc/PRO.h"
#include "abc/S.h"
#include "dog/git/CFG.h"

// --- SUBSu8sParse -----------------------------------------------------

ok64 SUBSu8sParse(u8cs blob, subs_cb cb, void *ctx) {
    sane(cb);

    a_pad(u8, cfgbuf,  4096);
    a_pad(u8, pathbuf,  512);
    a_pad(u8, urlbuf,  1024);

    CFGstate s = { .data = {blob[0], blob[1]}, .buf = cfgbuf };
    a_cstr(submod_word, "submodule");
    a_cstr(k_path,      "path");
    a_cstr(k_url,       "url");

    b8 in_submod = NO;

    for (;;) {
        ok64 o = CFGu8sFeed(&s);
        if (o == NODATA) break;
        if (o != OK) return SUBSPARSE;

        if (u8csEmpty(s.key)) {
            //  Section change: flush the previous submodule if any.
            if (in_submod) {
                a_dup(u8c, p, u8bDataC(pathbuf));
                a_dup(u8c, u, u8bDataC(urlbuf));
                if (!u8csEmpty(p) && !u8csEmpty(u)) {
                    ok64 cbo = cb(p, u, ctx);
                    if (cbo != OK) return cbo;
                }
                u8bReset(pathbuf);
                u8bReset(urlbuf);
            }
            in_submod = u8csEq(s.sec, submod_word);
            continue;
        }

        if (!in_submod) continue;
        if (u8csEq(s.key, k_path)) {
            u8bReset(pathbuf);
            call(u8bFeed, pathbuf, s.value);
        } else if (u8csEq(s.key, k_url)) {
            u8bReset(urlbuf);
            call(u8bFeed, urlbuf, s.value);
        }
    }

    //  Trailing flush.
    if (in_submod) {
        a_dup(u8c, p, u8bDataC(pathbuf));
        a_dup(u8c, u, u8bDataC(urlbuf));
        if (!u8csEmpty(p) && !u8csEmpty(u)) {
            ok64 cbo = cb(p, u, ctx);
            if (cbo != OK) return cbo;
        }
    }
    done;
}

// --- SUBSu8sFind ------------------------------------------------------

typedef struct {
    u8cs want_path;
    u8bp out_buf;       // caller's buffer, lives past Parse return
    ok64 err;           // sticky write error (NOROOM)
    b8   hit;
} subs_find_ctx;

//  Copy the URL bytes into the caller's buf so the resulting slice
//  outlives SUBSu8sParse's per-call stack scratch.  Returns OK; sets
//  err on NOROOM so SUBSu8sFind can surface it.
static ok64 subs_find_cb(u8cs path, u8cs url, void *vctx) {
    subs_find_ctx *c = (subs_find_ctx *)vctx;
    if (c->hit) return OK;
    if (!u8csEq(path, c->want_path)) return OK;
    u8bReset(c->out_buf);
    ok64 w = u8bFeed(c->out_buf, url);
    if (w != OK) { c->err = w; return w; }
    c->hit = YES;
    return OK;
}

ok64 SUBSu8sFind(u8cs blob, u8cs path, u8bp url_buf, u8csp url_out) {
    sane(url_buf);
    subs_find_ctx c = { .out_buf = url_buf, .err = OK };
    u8csMv(c.want_path, path);
    ok64 o = SUBSu8sParse(blob, subs_find_cb, &c);
    if (c.err != OK) return c.err;
    if (o != OK) return o;
    if (!c.hit) return SUBSNOSEC;
    a_dup(u8c, view, u8bData(url_buf));
    u8csMv(url_out, view);
    done;
}

// --- SUBSu8bSynth -----------------------------------------------------

ok64 SUBSu8bSynth(u8bp out, u8cs paths, u8cs urls) {
    sane(out);
    u8bReset(out);

    a_dup(u8c, p_scan, paths);
    a_dup(u8c, u_scan, urls);

    for (;;) {
        u8cs path = {}, url = {};
        if (u8csDrainLine(p_scan, path) != OK) break;
        if (u8csDrainLine(u_scan, url)  != OK) return SUBSPARSE;
        if (u8csEmpty(path) || u8csEmpty(url)) continue;

        call(u8bFeed, out, ((u8cs){(u8c *)"[submodule \"",
                                   (u8c *)"[submodule \"" + 12}));
        call(u8bFeed, out, path);
        call(u8bFeed, out, ((u8cs){(u8c *)"\"]\n\tpath = ",
                                   (u8c *)"\"]\n\tpath = " + 11}));
        call(u8bFeed, out, path);
        call(u8bFeed, out, ((u8cs){(u8c *)"\n\turl = ",
                                   (u8c *)"\n\turl = " + 8}));
        call(u8bFeed, out, url);
        call(u8bFeed1, out, '\n');
    }

    //  Trailing line not exhausted on the urls side → mismatched arity.
    if (!u8csEmpty(u_scan)) {
        u8cs trail = {};
        if (u8csDrainLine(u_scan, trail) == OK && !u8csEmpty(trail))
            return SUBSPARSE;
    }
    done;
}
