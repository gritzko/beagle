//  mark — StrictMark -> HTML renderer.  See README.mkd / INDEX.md.

#include "MARK.h"

#include "abc/PRO.h"
#include "abc/UTF8.h"
#include "dog/tok/MKDT.h"

#include <string.h>

#define MARK_MAX_REFS 512

//  A collected reference / shortcut definition: key -> url (into src).
typedef struct {
    u8cs key;
    u8cs url;
} markref;

//  Inline-rendering context, threaded through MKDTInlineLexer's callback.
#define MARK_MAX_DEPTH 64  // inline recursion cap (nested emphasis)

typedef struct {
    u8bp out;
    markref *refs;
    int nrefs;
    int depth;
    ok64 err;
} markctx;

//  -------- small helpers --------

ok64 MARKu8bLit(u8bp out, const char *s) {
    u8cs lit = {(u8c *)s, (u8c *)s + strlen(s)};
    return u8bFeed(out, lit);
}

static b8 mark_blank(u8csc s) {
    $for(u8c, p, s) {
        if (*p != ' ' && *p != '\t' && *p != '\r') return NO;
    }
    return YES;
}

//  Drain the next line from `cur`.  *linec = content without newline,
//  *linef = full line including newline.  Returns NO when exhausted.
static b8 mark_nextline(u8cs cur, u8csp linec, u8csp linef) {
    if (u8csEmpty(cur)) return NO;
    a_dup(u8c, scan, cur);
    ok64 f = u8csFind(scan, '\n');
    linec[0] = cur[0];
    linec[1] = scan[0];
    if (f == OK) u8csUsed(scan, 1);
    linef[0] = cur[0];
    linef[1] = scan[0];
    cur[0] = scan[0];
    return YES;
}

//  Parse a reference definition line "[key]: url ...".  Returns YES and
//  fills key/url on a hit.  Slice-level extraction, not a recursive parse.
static b8 mark_refdef(u8csc linec, u8csp key, u8csp url) {
    a_dup(u8c, s, linec);
    while (!u8csEmpty(s) && u8csAt(s, 0) == ' ') u8csUsed(s, 1);
    if (u8csEmpty(s) || u8csAt(s, 0) != '[') return NO;
    u8csUsed(s, 1);
    u8cs k = {s[0], s[0]};
    while (!u8csEmpty(s) && u8csAt(s, 0) != ']') u8csUsed(s, 1);
    k[1] = s[0];
    if (u8csEmpty(s)) return NO;
    u8csUsed(s, 1);  // past ']'
    if (u8csEmpty(s) || u8csAt(s, 0) != ':') return NO;
    u8csUsed(s, 1);  // past ':'
    while (!u8csEmpty(s) && (u8csAt(s, 0) == ' ' || u8csAt(s, 0) == '\t'))
        u8csUsed(s, 1);
    u8cs u = {s[0], s[0]};
    while (!u8csEmpty(s) && u8csAt(s, 0) != ' ' && u8csAt(s, 0) != '\t' &&
           u8csAt(s, 0) != '"' && u8csAt(s, 0) != '\r')
        u8csUsed(s, 1);
    u[1] = s[0];
    if (u8csEmpty(k) || u8csEmpty(u)) return NO;
    key[0] = k[0];
    key[1] = k[1];
    url[0] = u[0];
    url[1] = u[1];
    return YES;
}

//  Pass 1: collect every reference definition.
static void mark_collect_refs(u8csc src, markref *refs, int *nrefs) {
    a_dup(u8c, cur, src);
    u8cs linec = {}, linef = {};
    while (mark_nextline(cur, linec, linef) == YES) {
        if (*nrefs >= MARK_MAX_REFS) break;
        u8cs k = {}, u = {};
        if (mark_refdef(linec, k, u) == YES) {
            refs[*nrefs].key[0] = k[0];
            refs[*nrefs].key[1] = k[1];
            refs[*nrefs].url[0] = u[0];
            refs[*nrefs].url[1] = u[1];
            ++*nrefs;
        }
    }
}

static b8 mark_lookup(markctx *c, u8csc key, u8csp url) {
    for (int i = 0; i < c->nrefs; ++i) {
        if (u8csEq(c->refs[i].key, key)) {
            url[0] = c->refs[i].url[0];
            url[1] = c->refs[i].url[1];
            return YES;
        }
    }
    return NO;
}

//  Emit a URL (attribute-escaped), rewriting a trailing ".mkd" to ".html".
static ok64 mark_emit_url(u8bp out, u8csc url) {
    sane($ok(url));
    a_cstr(mkd, ".mkd");
    if (u8csHasSuffix(url, mkd)) {
        a_head(u8c, stem, url, u8csLen(url) - u8csLen(mkd));
        call(MARKu8bFeedEsc, out, stem);
        call(MARKu8bLit, out, ".html");
        done;
    }
    call(MARKu8bFeedEsc, out, url);
    done;
}

//  -------- inline rendering --------

static ok64 mark_inline(markctx *c, u8csc text);

static ok64 mark_emit_link(markctx *c, markg *g, b8 image) {
    sane(c != NULL);
    u8cs url = {};
    b8 found = mark_lookup(c, g->label, url);
    if (image) {
        call(MARKu8bLit, c->out, "<img src=\"");
        if (found) call(mark_emit_url, c->out, url);
        call(MARKu8bLit, c->out, "\" alt=\"");
        call(MARKu8bFeedEsc, c->out, g->text);
        call(MARKu8bLit, c->out, "\">");
        done;
    }
    call(MARKu8bLit, c->out, "<a href=\"");
    if (found) call(mark_emit_url, c->out, url);
    call(MARKu8bLit, c->out, "\">");
    call(MARKu8bFeedEsc, c->out, g->text);
    call(MARKu8bLit, c->out, "</a>");
    done;
}

//  Wrap inline `text` in an open/close HTML tag pair, recursing through
//  mark_inline (shared by the strong / em / del emphasis cases).
static ok64 mark_inline_wrap(markctx *c, u8csc text, const char *open,
                             const char *close) {
    c->err = MARKu8bLit(c->out, open);
    if (c->err == OK) c->err = mark_inline(c, text);
    if (c->err == OK) c->err = MARKu8bLit(c->out, close);
    return c->err;
}

static ok64 mark_inline_cb(u8 tag, u8cs tok, void *ctx) {
    markctx *c = (markctx *)ctx;
    if (c->err != OK) return c->err;

    if (tag == 'H') {  // inline code `...`
        a_dup(u8c, inner, tok);
        if (u8csLen(inner) >= 2) {
            u8csUsed(inner, 1);
            u8csShed1(inner);
        }
        c->err = MARKu8bLit(c->out, "<code>");
        if (c->err == OK) c->err = MARKu8bFeedEsc(c->out, inner);
        if (c->err == OK) c->err = MARKu8bLit(c->out, "</code>");
        return c->err;
    }

    if (tag == 'G') {  // emphasis / link / image
        markg g = {};
        MARKDecomposeG(&g, tok);
        switch (g.kind) {
            case 'B':
                return mark_inline_wrap(c, g.text, "<strong>", "</strong>");
            case 'I':
                return mark_inline_wrap(c, g.text, "<em>", "</em>");
            case 'D':
                return mark_inline_wrap(c, g.text, "<del>", "</del>");
            case 'A':
                c->err = mark_emit_link(c, &g, NO);
                return c->err;
            case 'M':
                c->err = mark_emit_link(c, &g, YES);
                return c->err;
            default:  // unrecognized: escape verbatim
                c->err = MARKu8bFeedEsc(c->out, tok);
                return c->err;
        }
    }

    //  S / P / L / W: literal text -> escape
    c->err = MARKu8bFeedEsc(c->out, tok);
    return c->err;
}

static ok64 mark_inline(markctx *c, u8csc text) {
    sane(c != NULL);
    if (u8csEmpty(text)) done;
    if (c->depth >= MARK_MAX_DEPTH) {  // too deep: stop recursing, emit literal
        c->err = MARKu8bFeedEsc(c->out, text);
        return c->err;
    }
    ++c->depth;
    MKDTstate ist = {
        .data = {(u8c *)text[0], (u8c *)text[1]},
        .cb = mark_inline_cb,
        .ctx = c,
    };
    ok64 o = MKDTInlineLexer(&ist);
    --c->depth;
    if (o != OK) c->err = o;
    return c->err;
}

//  -------- budget / structure validation --------

static ok64 mark_budget(markopts opts, const char *what, u8csc text,
                        size_t max) {
    size_t n = utf8CPLen((utf8c *const *)text);
    if (n > max) {
        fprintf(stderr, "mark: %s is %zu chars, exceeds %zu\n", what, n, max);
        if (opts.strict) return MARKLIMIT;
    }
    return OK;
}

static ok64 mark_violate(markopts opts, const char *msg) {
    fprintf(stderr, "mark: %s\n", msg);
    return opts.strict ? MARKLIMIT : OK;
}

//  -------- block rendering --------

//  Close one open leaf kind: if `flag` is set, emit its closing `tag` and
//  clear the flag (shared by paragraph / list / blockquote).
static ok64 mark_close(markctx *c, b8 *flag, const char *tag) {
    sane(c != NULL);
    if (*flag) {
        call(MARKu8bLit, c->out, tag);
        *flag = NO;
    }
    done;
}

//  Close an open list, choosing </ol> or </ul> by the ordered flag.
static ok64 mark_close_list(markctx *c, b8 *in_list, b8 *list_ord) {
    sane(c != NULL);
    if (*in_list) {
        call(MARKu8bLit, c->out, *list_ord ? "</ol>\n" : "</ul>\n");
        *in_list = NO;
        *list_ord = NO;
    }
    done;
}

//  Close the current leaf (paragraph/list/blockquote).  A leaf never spans
//  a block-stack depth change, a blank line, or a structural line.
static ok64 mark_close_leaf(markctx *c, b8 *in_para, b8 *in_list, b8 *list_ord,
                            b8 *in_quote) {
    sane(c != NULL);
    call(mark_close, c, in_para, "</p>\n");
    call(mark_close_list, c, in_list, list_ord);
    call(mark_close, c, in_quote, "</blockquote>\n");
    done;
}

//  Reconcile the open <div> nesting to `target`: a bare 4-space indent opens
//  a generic container, so each depth level is one <div>.  A depth change is
//  a hard leaf boundary, so the open leaf is closed first either way; then
//  surplus divs are closed (depth fell) or new divs opened (depth rose).
static ok64 mark_reconcile_divs(markctx *c, int *div_depth, int target,
                                b8 *in_para, b8 *in_list, b8 *list_ord,
                                b8 *in_quote) {
    sane(c != NULL);
    if (target == *div_depth) done;
    call(mark_close_leaf, c, in_para, in_list, list_ord, in_quote);
    while (*div_depth > target) {
        call(MARKu8bLit, c->out, "</div>\n");
        --*div_depth;
    }
    while (*div_depth < target) {
        call(MARKu8bLit, c->out, "<div>\n");
        ++*div_depth;
    }
    done;
}

static ok64 mark_blocks(markctx *c, u8csc src, markopts opts) {
    sane(c != NULL && $ok(src));

    a_dup(u8c, cur, src);
    u8cs linec = {}, linef = {};
    b8 in_fence = NO, in_para = NO, in_list = NO, list_ord = NO, in_quote = NO;
    b8 h1_seen = NO, opener = NO;
    int fence_len = 0;
    int div_depth = 0;  // open <div> nesting (one per 4-space indent level)

    while (mark_nextline(cur, linec, linef) == YES) {
        //  fenced code: copy raw, escaped, until the closing fence.
        if (in_fence) {
            if (MKDTFenceClose(linef, fence_len)) {
                in_fence = NO;
                call(MARKu8bLit, c->out, "</code></pre>\n");
            } else {
                call(MARKu8bFeedEsc, c->out, linec);
                call(MARKu8bLit, c->out, "\n");
            }
            continue;
        }
        int fl = MKDTFenceOpen(linef);
        if (fl > 0) {
            int fdepth = MKDTIndentDepth(linef);
            call(mark_reconcile_divs, c, &div_depth, fdepth, &in_para,
                 &in_list, &list_ord, &in_quote);
            in_fence = YES;
            fence_len = fl;
            call(MARKu8bLit, c->out, "<pre><code>");
            continue;
        }
        if (mark_blank(linec)) {
            //  A blank closes the open leaf; the surrounding div(s) persist so
            //  a blank-separated multi-paragraph div stays one container.  The
            //  next non-blank line reconciles div_depth to its own indent.
            call(mark_close_leaf, c, &in_para, &in_list, &list_ord, &in_quote);
            continue;
        }
        if (MKDTHRule(linef)) {
            int rdepth = MKDTIndentDepth(linef);
            call(mark_reconcile_divs, c, &div_depth, rdepth, &in_para,
                 &in_list, &list_ord, &in_quote);
            call(MARKu8bLit, c->out, "<hr>\n");
            continue;
        }
        {  // reference definition line: collected in pass 1, not rendered
            u8cs rk = {}, ru = {};
            if (mark_refdef(linec, rk, ru)) continue;
        }

        int hl = MKDTHeadingLevel(linef);
        if (hl > 0) {
            int depth = MKDTIndentDepth(linef);
            call(mark_reconcile_divs, c, &div_depth, depth, &in_para,
                 &in_list, &list_ord, &in_quote);
            a_dup(u8c, hc, linec);
            u8csUsed(hc, (size_t)depth * 4);
            for (int i = 0; i < hl && !u8csEmpty(hc) && u8csAt(hc, 0) == '#'; ++i)
                u8csUsed(hc, 1);
            while (!u8csEmpty(hc) && u8csAt(hc, 0) == ' ') u8csUsed(hc, 1);

            if (hl == 1) {
                if (h1_seen) call(mark_violate, opts, "more than one H1 opener");
                h1_seen = YES;
                opener = YES;  // next paragraph is the opener summary
            }
            call(mark_budget, opts, "header", hc, MARK_HEAD_MAX);

            char tag[5] = {'h', (char)('0' + hl), 0, 0, 0};
            call(MARKu8bLit, c->out, "<");
            call(MARKu8bLit, c->out, tag);
            call(MARKu8bLit, c->out, ">");
            call(mark_inline, c, hc);
            call(MARKu8bLit, c->out, "</");
            call(MARKu8bLit, c->out, tag);
            call(MARKu8bLit, c->out, ">\n");
            continue;
        }

        int depth = MKDTIndentDepth(linef);
        u8c *mend = NULL;
        mkdtmark mk = MKDTLineMarker(linef, depth, &mend);

        //  A depth change is a hard nesting boundary: open/close the div(s)
        //  (which also closes the open leaf) before rendering this line.
        call(mark_reconcile_divs, c, &div_depth, depth, &in_para, &in_list,
             &list_ord, &in_quote);

        if (mk == MKDT_MARK_ULIST || mk == MKDT_MARK_OLIST) {
            b8 want_ord = (mk == MKDT_MARK_OLIST);
            call(mark_close, c, &in_para, "</p>\n");
            call(mark_close, c, &in_quote, "</blockquote>\n");
            //  Switching between bullets and numbers ends one list, starts
            //  the other; same kind just continues the open list.
            if (in_list && list_ord != want_ord)
                call(mark_close_list, c, &in_list, &list_ord);
            if (!in_list) {
                call(MARKu8bLit, c->out, want_ord ? "<ol>\n" : "<ul>\n");
                in_list = YES;
                list_ord = want_ord;
            }
            u8cs bc = {mend, linec[1]};
            call(mark_budget, opts, "bullet", bc, MARK_BULLET_MAX);
            call(MARKu8bLit, c->out, "<li>");
            call(mark_inline, c, bc);
            call(MARKu8bLit, c->out, "</li>\n");
            opener = NO;
            continue;
        }
        if (mk == MKDT_MARK_QUOTE) {
            call(mark_close, c, &in_para, "</p>\n");
            call(mark_close_list, c, &in_list, &list_ord);
            if (!in_quote) {
                call(MARKu8bLit, c->out, "<blockquote>\n");
                in_quote = YES;
            }
            u8cs qc = {mend, linec[1]};
            call(mark_inline, c, qc);
            call(MARKu8bLit, c->out, "\n");
            opener = NO;
            continue;
        }

        //  paragraph / summary.  Strip the indent gutter: the marker is NONE
        //  for a bare div, so `mend` points just past the depth*4 indents.
        u8cs pc = {mend, linec[1]};
        call(mark_close_list, c, &in_list, &list_ord);
        call(mark_close, c, &in_quote, "</blockquote>\n");
        if (!in_para) {
            call(MARKu8bLit, c->out, "<p>\n");
            in_para = YES;
            call(mark_budget, opts, opener ? "opener summary" : "summary",
                 pc, opener ? MARK_OPEN_MAX : MARK_SUMM_MAX);
            opener = NO;
        }
        call(mark_inline, c, pc);
        call(MARKu8bLit, c->out, "\n");
    }

    //  EOF: close the open leaf and unwind every remaining div.
    call(mark_close_leaf, c, &in_para, &in_list, &list_ord, &in_quote);
    call(mark_reconcile_divs, c, &div_depth, 0, &in_para, &in_list, &list_ord,
         &in_quote);
    if (in_fence) call(MARKu8bLit, c->out, "</code></pre>\n");
    if (!h1_seen) call(mark_violate, opts, "no H1 opener (one concept per page)");
    done;
}

//  -------- document --------

static ok64 mark_body(u8bp out, u8csc src, markopts opts) {
    sane($ok(src));
    markref refs[MARK_MAX_REFS];
    int nrefs = 0;
    mark_collect_refs(src, refs, &nrefs);
    markctx c = {.out = out, .refs = refs, .nrefs = nrefs, .err = OK};
    call(mark_blocks, &c, src, opts);
    done;
}

ok64 MARKRenderDoc(u8bp out, u8csc src, u8csc title, markopts opts) {
    sane($ok(src) && out != NULL);
    call(MARKu8bLit, out,
         "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
         "<meta charset=\"utf-8\">\n"
         "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
         "<title>");
    if (!$empty(title)) {
        call(MARKu8bFeedEsc, out, title);
    } else {
        call(MARKu8bLit, out, "wiki");
    }
    call(MARKu8bLit, out, "</title>\n");
    //  Site-owned head extras (stylesheet, favicon, meta...) carried in
    //  --head=FILE; mark itself bakes in no page policy.
    if (!$empty(opts.head)) call(u8bFeed, out, opts.head);
    call(MARKu8bLit, out, "</head>\n<body>\n");
    //  Site-owned body lead-in (banner, nav...) carried in --body=FILE,
    //  injected verbatim before the page content; mark bakes in no policy.
    if (!$empty(opts.body)) call(u8bFeed, out, opts.body);
    call(mark_body, out, src, opts);
    call(MARKu8bLit, out, "</body>\n</html>\n");
    done;
}
