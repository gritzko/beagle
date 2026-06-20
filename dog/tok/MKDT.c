#include "MKDT.h"

#include "abc/PRO.h"
#include "dog/tok/MDBLK.h"

// Inline ragel lexer (MKDT.rl.c, generated from MKDT.c.rl)
ok64 MKDTInlineLexer(MKDTstate *state);

// --- Inline callbacks ---

ok64 MKDTonEmph(u8cs tok, MKDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 MKDTonCode(u8cs tok, MKDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 MKDTonLink(u8cs tok, MKDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 MKDTonNumber(u8cs tok, MKDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 MKDTonWord(u8cs tok, MKDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('S', tok, state->ctx);
    done;
}

ok64 MKDTonPunct(u8cs tok, MKDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 MKDTonSpace(u8cs tok, MKDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}

//  Backslash escape \<punct>: emit the literal run with the leading
//  backslash dropped, as plain punctuation text (kind 'P'). The escaped
//  opener has already cancelled its bracketing role in the lexer, so the
//  whole run reaches HTML verbatim (escaped), never as an inline span.
ok64 MKDTonEscape(u8cs tok, MKDTstate *state) {
    sane($ok(tok) && state != NULL);
    a_dup(u8c, lit, tok);
    u8csUsed(lit, 1);  //  drop the leading backslash
    if (state->cb) return state->cb('P', lit, state->ctx);
    done;
}

// --- Block-line classifiers (thin wrappers over the MKDTB block grammar) ---

int MKDTFenceOpen(u8csc line) {
    mkdtblock b;
    MKDTBlock(line, &b);
    return (b.fence == 3 || b.fence == 4) ? b.fence : 0;
}

b8 MKDTFenceClose(u8csc line, int flen) {
    mkdtblock b;
    MKDTBlock(line, &b);
    return (b.fence >= flen && b.fence_blank) ? YES : NO;
}

int MKDTHeadingLevel(u8csc line) {
    mkdtblock b;
    MKDTBlock(line, &b);
    return b.heading;
}

b8 MKDTHRule(u8csc line) {
    mkdtblock b;
    MKDTBlock(line, &b);
    return b.hrule;
}

b8 MKDTRefDef(u8csc line) {
    mkdtblock b;
    MKDTBlock(line, &b);
    return b.refdef;
}

int MKDTIndentDepth(u8csc line) {
    mkdtblock b;
    MKDTBlock(line, &b);
    return b.depth;
}

mkdtmark MKDTLineMarker(u8csc line, int depth, u8c **markend) {
    (void)depth;
    mkdtblock b;
    MKDTBlock(line, &b);
    *markend = (u8c *)b.content;
    return b.marker;
}

// Emit heading: the "#... " markup as R, then the content through inline.
static ok64 MKDTEmitHeading(MKDTstate *state, u8csc line) {
    u8c *e = (u8c *)line[1];
    mkdtblock b;
    MKDTBlock(line, &b);
    u8c *p = (u8c *)b.content;   // first content byte after the header markup

    u8cs prefix = {line[0], p};
    if (state->cb && !$empty(prefix)) {
        ok64 o = TOKSplitText('R', prefix, state->cb, state->ctx);
        if (o != OK) return o;
    }

    u8c *ce = e;
    b8 has_nl = NO;
    u8cs body = {p, e};
    if (!u8csEmpty(body) && *u8csLast(body) == '\n') { ce--; has_nl = YES; }

    if (p < ce) {
        MKDTstate ist = {.data = {p, ce}, .cb = state->cb, .ctx = state->ctx};
        ok64 o = MKDTInlineLexer(&ist);
        if (o != OK) return o;
    }

    if (has_nl && state->cb) {
        u8cs nl = {ce, e};
        ok64 o = state->cb('S', nl, state->ctx);
        if (o != OK) return o;
    }
    return OK;
}

// --- Block-level lexer (drives the MKDTB grammar, one classification/line) ---

ok64 MKDTLexer(MKDTstate *state) {
    sane($ok(state->data) && state != NULL);

    a_dup(u8c, scan, state->data);
    b8 in_fence = NO;
    int fence_len = 0;

    u8cs line = {};
    while (MDBLKu8csDrainLine(scan, line) == OK) {
        u8c *sol = (u8c *)line[0];   // start of line
        u8c *cur = (u8c *)line[1];   // end of line (past the '\n')
        mkdtblock b;
        MKDTBlock(line, &b);

        if (in_fence) {
            if (b.fence >= fence_len && b.fence_blank) in_fence = NO;
            if (state->cb) {
                ok64 o = state->cb('H', line, state->ctx);
                if (o != OK) { state->data[0] = scan[0]; return o; }
            }
            continue;
        }

        if (b.fence == 3 || b.fence == 4) {
            in_fence = YES;
            fence_len = b.fence;
            if (state->cb) {
                ok64 o = state->cb('H', line, state->ctx);
                if (o != OK) { state->data[0] = scan[0]; return o; }
            }
        } else if (b.hrule) {
            if (state->cb) {
                ok64 o = TOKSplitText('R', line, state->cb, state->ctx);
                if (o != OK) { state->data[0] = scan[0]; return o; }
            }
        } else if (b.refdef) {
            if (state->cb) {
                ok64 o = TOKSplitText('R', line, state->cb, state->ctx);
                if (o != OK) { state->data[0] = scan[0]; return o; }
            }
        } else if (b.heading > 0) {
            ok64 o = MKDTEmitHeading(state, line);
            if (o != OK) { state->data[0] = scan[0]; return o; }
        } else {
            // Paragraph / list / blockquote / div: indent+marker markup as R,
            // then the inline machine on the rest.
            u8c *text_start = (u8c *)b.content;
            if (text_start > sol && state->cb) {
                u8cs markup = {sol, text_start};
                ok64 o = TOKSplitText('R', markup, state->cb, state->ctx);
                if (o != OK) { state->data[0] = scan[0]; return o; }
            }
            if (text_start < cur) {
                MKDTstate ist = {
                    .data = {text_start, cur},
                    .cb = state->cb,
                    .ctx = state->ctx,
                };
                ok64 o = MKDTInlineLexer(&ist);
                if (o != OK) { state->data[0] = scan[0]; return o; }
            }
        }
    }

    state->data[0] = scan[0];
    return OK;
}
