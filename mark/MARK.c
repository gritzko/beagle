//  mark — StrictMark -> HTML renderer.  See README.mkd / INDEX.md.

#include "MARK.h"

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/UTF8.h"
#include "dog/tok/MKDT.h"

#include <string.h>

#define MARK_MAX_REFS 512

//  Slice literals reused across the renderer (one definition each, not
//  re-declared per function).  u8slit is a compile-time slice initializer.
static u8cs s_ext_mkd  = u8slit("mkd");
static u8cs s_ext_md   = u8slit("md");
static u8cs s_dot_mkd  = u8slit(".mkd");
static u8cs s_dot_md   = u8slit(".md");
static u8cs s_dot_html = u8slit(".html");

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
    u8bp para;        // soft-wrapped paragraph text, joined into one line
    b8 para_opener;   // YES if the buffered paragraph is the H1 opener summary
    markopts opts;    // budget options, applied at paragraph flush time
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
    if (u8csHasSuffix(url, s_dot_mkd)) {
        a_head(u8c, stem, url, u8csLen(url) - u8csLen(s_dot_mkd));
        call(MARKu8bFeedEsc, out, stem);
        call(MARKu8bLit, out, ".html");
        done;
    }
    call(MARKu8bFeedEsc, out, url);
    done;
}

//  -------- inline rendering --------

static ok64 mark_inline(markctx *c, u8csc text);

//  YES if `ext` is the page source extension (.mkd / .md, sans dot).
static b8 mark_is_pageext(u8csc ext) {
    return !u8csEmpty(ext) && (u8csEq(ext, s_ext_mkd) || u8csEq(ext, s_ext_md));
}

//  Trim a trailing ".mkd" / ".md" from slice `s` (the page source extension);
//  any other extension is left intact (it names a real asset, not a page).
static void mark_trim_pageext(u8csp s) {
    u8cs v = {s[0], s[1]};
    u8cs ext = {};
    PATHu8sExt(ext, v);
    if (mark_is_pageext(ext)) {
        a_tail(u8c, suf, v, u8csLen(ext) + 1);  // ".mkd" / ".md"
        s[1] = suf[0];
    }
}

//  YES if the file root/stem+dotext exists.
static b8 mark_page_at(u8csc root, u8csc stem, u8csc dotext) {
    a_path(p, root);
    if (PATHu8bAdd(p, stem) != OK) return NO;
    if (PATHu8bFeed(p, dotext) != OK) return NO;
    return FILEExists($path(p)) == OK;
}

//  YES if a source page exists at root-relative `stem` under `root`
//  (probed as stem.mkd, then stem.md).  Empty root => NO (no tree to probe).
static b8 mark_page_exists(u8csc root, u8csc stem) {
    if (u8csEmpty(root) || u8csEmpty(stem)) return NO;
    return mark_page_at(root, stem, s_dot_mkd) ||
           mark_page_at(root, stem, s_dot_md);
}

//  Emit the href for an unresolved absolute bracket target (a path/wiki link).
//  The leading slash is meaningful: the href stays root-absolute, so the same
//  link resolves identically on every page regardless of its depth (the site
//  is served from the domain root).  The target is normalized, then the
//  extension is decided: an explicit .mkd/.md, or an extensionless target
//  whose .mkd/.md source exists under opts.root, resolves to .html; anything
//  else (a directory, /LICENSE, /img/x.png) is emitted verbatim.
static ok64 mark_emit_pathlink(markctx *c, u8csc bracket) {
    sane(c != NULL);

    //  tgt = the target as an absolute, normalized path.  `bracket` is always
    //  root-absolute here (callers gate on a leading '/').
    a_dup(u8c, br, bracket);
    a_path(tgt);
    call(PATHu8bNorm, tgt, br);

    //  Decide page (-> .html) vs verbatim, build the final root-absolute href.
    a_dup(u8c, tv, u8bDataC(tgt));
    u8cs ext = {};
    PATHu8sExt(ext, tv);
    b8 ext_page = mark_is_pageext(ext);
    b8 topage = ext_page;
    if (!topage && u8csEmpty(ext)) {
        a_rest(u8c, stem, tv, 1);  // drop leading '/': root-relative stem
        topage = mark_page_exists(c->opts.root, stem);
    }
    a_path(fin);
    if (topage) {
        a_dup(u8c, stem, tv);
        if (ext_page) {
            a_tail(u8c, suf, stem, u8csLen(ext) + 1);
            stem[1] = suf[0];
        }
        call(PATHu8bDup, fin, stem);
        call(PATHu8bFeed, fin, s_dot_html);
    } else {
        call(PATHu8bDup, fin, tv);
    }

    a_dup(u8c, finv, u8bDataC(fin));
    call(MARKu8bFeedEsc, c->out, finv);
    done;
}

static ok64 mark_emit_link(markctx *c, mkdtspan *g, b8 image) {
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

    //  Only an undefined *absolute* shortcut `[/path]` is a path link.  A
    //  shortcut [text] keys on its own bracket text (label == text); a
    //  reference link [text][L] has a distinct one-symbol label.  Any other
    //  undefined bracket keeps the prior behavior (empty href); a literal
    //  bracket is written by escaping it (`\[42]`), not auto-detected here.
    b8 shortcut = g->label[0] == g->text[0] && g->label[1] == g->text[1];
    a_dup(u8c, t, g->text);
    b8 pathlink = !found && shortcut && PATHu8sIsAbsolute(t);

    call(MARKu8bLit, c->out, "<a href=\"");
    if (found) {
        call(mark_emit_url, c->out, url);
    } else if (pathlink) {
        call(mark_emit_pathlink, c, g->text);
    }
    call(MARKu8bLit, c->out, "\">");
    if (pathlink) {
        //  Display the basename (page extension dropped), so
        //  `[/wiki/StrictMark]` reads "StrictMark" and a trailing suffix
        //  (`[/wiki/Submodule]s`) glues on outside the anchor.
        a_dup(u8c, disp, g->text);
        u8cs base = {};
        PATHu8sBase(base, disp);
        mark_trim_pageext(base);
        call(MARKu8bFeedEsc, c->out, base);
    } else {
        call(MARKu8bFeedEsc, c->out, g->text);
    }
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
        mkdtspan g = {};
        MKDTDecomposeSpan(&g, tok);
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

//  The block container stack mirrors StrictMark's `(INDENT|QUOTE)* LIST?`: one
//  frame per 4-char indent group, frame i holding depth-(i+1) content.  A bare
//  indent is a <div>; a list marker is a <ul>/<ol> whose <li> stays open (its
//  `</li>` deferred) so a deeper-indented list or paragraph nests *inside* the
//  item rather than closing the list and becoming a sibling.
#define MARK_MAX_STACK 64
#define MARK_STACK_CAP (MARK_MAX_STACK - 2)  // leave room for a marker frame

typedef enum { MARK_DIV, MARK_UL, MARK_OL, MARK_QUOTE, MARK_TODOL } markckind;
typedef struct {
    markckind kind;
    b8        li_open;  // list only: an <li> is open, its </li> not yet emitted
} markframe;

//  Flush the buffered paragraph: open <p>, budget-check the whole (possibly
//  soft-wrapped) text, render its inline content, close </p>.  Soft line
//  breaks were joined as single spaces while accumulating, so an inline span
//  (link, image, emphasis) may cross a source line wrap.
static ok64 mark_para_flush(markctx *c, b8 *in_para) {
    sane(c != NULL);
    if (!*in_para) done;
    a_dup(u8c, pc, u8bDataC(c->para));
    call(MARKu8bLit, c->out, "<p>\n");
    call(mark_budget, c->opts, c->para_opener ? "opener summary" : "summary",
         pc, c->para_opener ? MARK_OPEN_MAX : MARK_SUMM_MAX);
    call(mark_inline, c, pc);
    call(MARKu8bLit, c->out, "\n</p>\n");
    u8bReset(c->para);
    *in_para = NO;
    c->para_opener = NO;
    done;
}

//  Close the innermost container, emitting its end tag(s); a list first closes
//  its open <li>.  The caller has already flushed any buffered paragraph.
static ok64 mark_pop(markctx *c, markframe *stk, int *nstk) {
    sane(c != NULL && *nstk > 0);
    markframe f = stk[--*nstk];
    if (f.kind == MARK_DIV) {
        call(MARKu8bLit, c->out, "</div>\n");
    } else if (f.kind == MARK_QUOTE) {
        call(MARKu8bLit, c->out, "</blockquote>\n");
    } else {  // MARK_UL / MARK_OL / MARK_TODOL (a todo list is a <ul>)
        if (f.li_open) call(MARKu8bLit, c->out, "</li>\n");
        call(MARKu8bLit, c->out, f.kind == MARK_OL ? "</ol>\n" : "</ul>\n");
    }
    done;
}

//  Close containers until the stack holds at most `n` frames, flushing the
//  buffered paragraph first (it lives in the innermost frame being closed).
static ok64 mark_unwind(markctx *c, markframe *stk, int *nstk, int n,
                        b8 *in_para) {
    sane(c != NULL);
    if (*nstk > n) call(mark_para_flush, c, in_para);
    while (*nstk > n) call(mark_pop, c, stk, nstk);
    done;
}

//  Grow the stack to `depth` frames with generic <div> containers (the bare
//  4-space indent), so an ancestor level a marker/leaf needs but that no list
//  or quote occupies still nests.
static ok64 mark_grow_divs(markctx *c, markframe *stk, int *nstk, int depth) {
    sane(c != NULL);
    while (*nstk < depth) {
        call(MARKu8bLit, c->out, "<div>\n");
        stk[*nstk].kind = MARK_DIV;
        stk[*nstk].li_open = NO;
        ++*nstk;
    }
    done;
}

//  Reconcile the stack for a leaf (paragraph/header/hr/fence) at indent `depth`:
//  a leaf has no child container, so close everything at index >= depth, then
//  open <div> ancestors up to `depth`.  The leaf then renders inside frame
//  depth-1 (a list's open <li>, a div, or the document at depth 0).
static ok64 mark_enter_leaf(markctx *c, markframe *stk, int *nstk, int depth,
                            b8 *in_para) {
    sane(c != NULL);
    call(mark_unwind, c, stk, nstk, depth, in_para);
    call(mark_grow_divs, c, stk, nstk, depth);
    done;
}

//  Open or continue a list at indent `depth` and emit one <li> with `item`
//  (its </li> deferred).  A same-kind list already at that level continues with
//  a new sibling item; otherwise the level is (re)opened, nesting inside the
//  enclosing item when `depth` is deeper than the current stack.
static ok64 mark_enter_list(markctx *c, markframe *stk, int *nstk, int depth,
                            b8 ord, u8csc item, b8 *in_para) {
    sane(c != NULL);
    markckind want = ord ? MARK_OL : MARK_UL;
    call(mark_para_flush, c, in_para);  // emit any item-trailing paragraph first
    call(mark_unwind, c, stk, nstk, depth + 1, in_para);  // close deeper levels
    b8 reuse = (*nstk == depth + 1) && stk[depth].kind == want;
    if (*nstk == depth + 1 && !reuse)  // a different container holds this level
        call(mark_unwind, c, stk, nstk, depth, in_para);
    if (reuse) {  // sibling item: close the current item, open the next
        if (stk[depth].li_open) call(MARKu8bLit, c->out, "</li>\n");
    } else {
        call(mark_grow_divs, c, stk, nstk, depth);
        call(MARKu8bLit, c->out, want == MARK_OL ? "<ol>\n" : "<ul>\n");
        stk[*nstk].kind = want;
        stk[*nstk].li_open = NO;
        ++*nstk;
    }
    call(mark_budget, c->opts, "bullet", item, MARK_BULLET_MAX);
    call(MARKu8bLit, c->out, "<li>");
    call(mark_inline, c, item);
    stk[depth].li_open = YES;
    done;
}

//  Open or continue a blockquote at indent `depth` and emit one quoted line.
static ok64 mark_enter_quote(markctx *c, markframe *stk, int *nstk, int depth,
                             u8csc content, b8 *in_para) {
    sane(c != NULL);
    call(mark_para_flush, c, in_para);
    call(mark_unwind, c, stk, nstk, depth + 1, in_para);
    b8 reuse = (*nstk == depth + 1) && stk[depth].kind == MARK_QUOTE;
    if (*nstk == depth + 1 && !reuse)
        call(mark_unwind, c, stk, nstk, depth, in_para);
    if (!reuse) {
        call(mark_grow_divs, c, stk, nstk, depth);
        call(MARKu8bLit, c->out, "<blockquote>\n");
        stk[*nstk].kind = MARK_QUOTE;
        stk[*nstk].li_open = NO;
        ++*nstk;
    }
    call(mark_inline, c, content);
    call(MARKu8bLit, c->out, "\n");
    done;
}

//  Open or continue a TODO list (`<ul class="todo">`) at indent `depth` and emit
//  one item with a disabled checkbox reflecting `state` (the char inside the
//  `-[·]` marker).  A checkbox is binary, so the four states ride on the <li>
//  class: ' ' open, 'v'/'V' done (checked), '-' blocked, 'x'/'X' wontfix (the
//  content also struck through with <del>).  The </li> is deferred like a list.
static ok64 mark_enter_todo(markctx *c, markframe *stk, int *nstk, int depth,
                            u8 state, u8csc item, b8 *in_para) {
    sane(c != NULL);
    call(mark_para_flush, c, in_para);
    call(mark_unwind, c, stk, nstk, depth + 1, in_para);
    b8 reuse = (*nstk == depth + 1) && stk[depth].kind == MARK_TODOL;
    if (*nstk == depth + 1 && !reuse)
        call(mark_unwind, c, stk, nstk, depth, in_para);
    if (reuse) {
        if (stk[depth].li_open) call(MARKu8bLit, c->out, "</li>\n");
    } else {
        call(mark_grow_divs, c, stk, nstk, depth);
        call(MARKu8bLit, c->out, "<ul class=\"todo\">\n");
        stk[*nstk].kind = MARK_TODOL;
        stk[*nstk].li_open = NO;
        ++*nstk;
    }
    const char *cls = "open";
    b8 checked = NO, del = NO;
    if (state == 'v' || state == 'V') { cls = "done"; checked = YES; }
    else if (state == '-') { cls = "blocked"; }
    else if (state == 'x' || state == 'X') { cls = "wontfix"; del = YES; }
    call(mark_budget, c->opts, "bullet", item, MARK_BULLET_MAX);
    call(MARKu8bLit, c->out, "<li class=\"");
    call(MARKu8bLit, c->out, cls);
    call(MARKu8bLit, c->out, "\"><input type=\"checkbox\"");
    if (checked) call(MARKu8bLit, c->out, " checked");
    call(MARKu8bLit, c->out, " disabled> ");
    if (del) call(MARKu8bLit, c->out, "<del>");
    call(mark_inline, c, item);
    if (del) call(MARKu8bLit, c->out, "</del>");
    stk[depth].li_open = YES;
    done;
}

static ok64 mark_blocks(markctx *c, u8csc src, markopts opts) {
    sane(c != NULL && $ok(src));

    //  Scratch for the running paragraph; a joined paragraph is never longer
    //  than the source (newlines collapse to spaces, gutters are dropped).
    a_carve(u8, para, u8csLen(src) + 8);
    c->para = para;
    c->opts = opts;

    a_dup(u8c, cur, src);
    u8cs linec = {}, linef = {};
    markframe stk[MARK_MAX_STACK];
    int nstk = 0;  // open container frames (divs / lists / quotes)
    b8 in_fence = NO, in_para = NO;
    b8 h1_seen = NO, opener = NO;
    int fence_len = 0;

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
            if (fdepth > MARK_STACK_CAP) fdepth = MARK_STACK_CAP;
            call(mark_para_flush, c, &in_para);
            call(mark_enter_leaf, c, stk, &nstk, fdepth, &in_para);
            in_fence = YES;
            fence_len = fl;
            call(MARKu8bLit, c->out, "<pre><code>");
            continue;
        }
        if (mark_blank(linec)) {
            //  A blank flushes the paragraph and closes any open list/quote
            //  leaves; enclosing <div>s persist so a blank-separated
            //  multi-paragraph div stays one container.
            call(mark_para_flush, c, &in_para);
            while (nstk > 0 && stk[nstk - 1].kind != MARK_DIV)
                call(mark_pop, c, stk, &nstk);
            continue;
        }
        if (MKDTHRule(linef)) {
            int rdepth = MKDTIndentDepth(linef);
            if (rdepth > MARK_STACK_CAP) rdepth = MARK_STACK_CAP;
            call(mark_para_flush, c, &in_para);
            call(mark_enter_leaf, c, stk, &nstk, rdepth, &in_para);
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
            if (depth > MARK_STACK_CAP) depth = MARK_STACK_CAP;
            call(mark_para_flush, c, &in_para);
            call(mark_enter_leaf, c, stk, &nstk, depth, &in_para);
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
        if (depth > MARK_STACK_CAP) depth = MARK_STACK_CAP;
        u8c *mend = NULL;
        mkdtmark mk = MKDTLineMarker(linef, depth, &mend);

        if (mk == MKDT_MARK_ULIST || mk == MKDT_MARK_OLIST) {
            u8cs bc = {mend, linec[1]};
            call(mark_enter_list, c, stk, &nstk, depth,
                 mk == MKDT_MARK_OLIST, bc, &in_para);
            opener = NO;
            continue;
        }
        if (mk == MKDT_MARK_QUOTE) {
            u8cs qc = {mend, linec[1]};
            call(mark_enter_quote, c, stk, &nstk, depth, qc, &in_para);
            opener = NO;
            continue;
        }
        if (mk == MKDT_MARK_TODO) {
            //  state char sits inside the `-[·]` block, at indent+2.
            u8 state = linef[0][(size_t)depth * 4 + 2];
            u8cs tc = {mend, linec[1]};
            call(mark_enter_todo, c, stk, &nstk, depth, state, tc, &in_para);
            opener = NO;
            continue;
        }

        //  paragraph / summary.  Strip the indent gutter: the marker is NONE
        //  for a bare div, so `mend` points just past the depth*4 indents.
        //  Soft-wrapped lines join into one logical line (each newline a space)
        //  so an inline span may cross the wrap; the flush at the next leaf
        //  boundary renders and budget-checks the whole.  A continuation line
        //  (already mid-paragraph at the same depth) just appends.
        u8cs pc = {mend, linec[1]};
        b8 cont = in_para && nstk == depth;
        if (!cont) {
            call(mark_para_flush, c, &in_para);
            call(mark_enter_leaf, c, stk, &nstk, depth, &in_para);
            c->para_opener = opener;
            opener = NO;
        } else {
            call(u8bFeed1, c->para, ' ');
        }
        call(u8bFeed, c->para, pc);
        in_para = YES;
    }

    //  EOF: flush the open paragraph and unwind every remaining container.
    call(mark_para_flush, c, &in_para);
    call(mark_unwind, c, stk, &nstk, 0, &in_para);
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
