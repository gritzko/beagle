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

// --- Block-level helpers ---

// Check if line is a StrictMark code fence (3-4 backticks after div markup).
// Returns fence length (3 or 4) or 0.
int MKDTFenceOpen(u8csc line) {
    a_dup(u8c, c, line);
    MDBLKu8csSkipIndents(c);          // skip div markup
    if (u8csEmpty(c) || *u8csHead(c) != '`') return 0;
    int count = MDBLKu8csRun(c, '`');
    if (count < 3 || count > 4) return 0;
    return count;
}

// Check if line closes a fenced code block.
b8 MKDTFenceClose(u8csc line, int flen) {
    a_dup(u8c, c, line);
    MDBLKu8csSkipIndents(c);
    if (u8csEmpty(c) || *u8csHead(c) != '`') return NO;
    int count = MDBLKu8csRun(c, '`');
    if (count < flen) return NO;
    return MDBLKu8csAllBlank(c);
}

// Check ATX heading level (1-4 only), with 4-char-wide markup.
// Returns level or 0.
int MKDTHeadingLevel(u8csc line) {
    a_dup(u8c, c, line);
    MDBLKu8csSkipIndents(c);          // skip nesting
    if (u8csEmpty(c) || *u8csHead(c) != '#') return 0;
    int level = MDBLKu8csRun(c, '#');
    if (level > 4) return 0;
    return level;
}

// Check horizontal rule: a 3-dash "---" (the short case) or 4-dash "----".
// Per StrictMark: "---"/"--- "/"----" is a ruler; structural markup is
// 4-char-wide except two shorter cases, one being the 3-dash ruler.
b8 MKDTHRule(u8csc line) {
    a_dup(u8c, c, line);
    MDBLKu8csSkipIndents(c);
    // Need at least three dashes.
    if (u8csLen(c) < 3 || c[0][0] != '-' || c[0][1] != '-' || c[0][2] != '-')
        return NO;
    u8csUsed(c, 3);
    // Optional fourth dash (the 4-char-wide form), then the gap.
    if (!u8csEmpty(c) && *u8csHead(c) == '-') u8csUsed1(c);
    // Rest must be whitespace/newline
    return MDBLKu8csAllBlank(c);
}

// Check reference definition: [x]: ...
b8 MKDTRefDef(u8csc line) {
    a_dup(u8c, c, line);
    MDBLKu8csSkipIndents(c);
    if (u8csLen(c) < 4) return NO;
    if (c[0][0] != '[') return NO;
    u8 ch = c[0][1];
    b8 alnum = (ch >= '0' && ch <= '9') ||
               (ch >= 'A' && ch <= 'Z') ||
               (ch >= 'a' && ch <= 'z');
    if (!alnum) return NO;
    if (c[0][2] != ']' || c[0][3] != ':') return NO;
    return YES;
}

// Count leading 4-space indent blocks (div markup depth).
int MKDTIndentDepth(u8csc line) {
    a_dup(u8c, c, line);
    return MDBLKu8csSkipIndents(c);
}

// Classify the block marker in the 4-char group after `depth` indents.
mkdtmark MKDTLineMarker(u8csc line, int depth, u8c **markend) {
    a_dup(u8c, c, line);
    u8csUsed(c, (size_t)depth * 4);   // step past the indent blocks
    *markend = (u8c *)c[0];
    if (u8csLen(c) < 4) return MKDT_MARK_NONE;
    u8c *content = (u8c *)c[0];       // first byte of the 4-char group
    u8c *group_end = content + 4;     // 4-char marker slot end
    // Blockquote: >___
    if (content[0] == '>' ||
        (content[0] == ' ' && content[1] == '>') ||
        (content[0] == ' ' && content[1] == ' ' && content[2] == '>') ||
        (content[0] == ' ' && content[1] == ' ' && content[2] == ' ' &&
         content[3] == '>')) {
        *markend = group_end;
        return MKDT_MARK_QUOTE;
    }
    // Unordered list: -___
    if (content[0] == '-' ||
        (content[0] == ' ' && content[1] == '-') ||
        (content[0] == ' ' && content[1] == ' ' && content[2] == '-') ||
        (content[0] == ' ' && content[1] == ' ' && content[2] == ' ' &&
         content[3] == '-')) {
        *markend = group_end;
        return MKDT_MARK_ULIST;
    }
    // Ordered list: N. or NN. etc
    if ((content[0] >= '0' && content[0] <= '9') ||
        (content[0] == ' ' && content[1] >= '0' && content[1] <= '9')) {
        u8cs g = {content, group_end};
        MDBLKu8csRun(g, ' ');
        while (!u8csEmpty(g) && *u8csHead(g) >= '0' && *u8csHead(g) <= '9')
            u8csUsed1(g);
        if (!u8csEmpty(g) && *u8csHead(g) == '.') {
            *markend = group_end;
            return MKDT_MARK_OLIST;
        }
    }
    // TODO: [ ] [x] [X]
    if (content[0] == '[' &&
        (content[1] == ' ' || content[1] == 'x' || content[1] == 'X') &&
        content[2] == ']' && content[3] == ' ') {
        *markend = group_end;
        return MKDT_MARK_TODO;
    }
    return MKDT_MARK_NONE;
}

// Emit heading: prefix (div markup + #+ space) as R, content through inline.
static ok64 MKDTEmitHeading(MKDTstate *state, u8csc line) {
    u8c *e = (u8c *)line[1];
    a_dup(u8c, c, line);

    // Skip div markup, consume # markers, then trailing spaces in 4-char block
    MDBLKu8csSkipIndents(c);
    MDBLKu8csRun(c, '#');
    MDBLKu8csRun(c, ' ');
    u8c *p = (u8c *)c[0];   // first content byte

    // Emit prefix as R
    u8cs prefix = {line[0], p};
    if (state->cb && !$empty(prefix)) {
        ok64 o = TOKSplitText('R', prefix, state->cb, state->ctx);
        if (o != OK) return o;
    }

    // Strip trailing newline for content
    u8c *ce = e;
    b8 has_nl = NO;
    u8cs body = {p, e};
    if (!u8csEmpty(body) && *u8csLast(body) == '\n') { ce--; has_nl = YES; }

    // Run inline on heading content
    if (p < ce) {
        MKDTstate ist = {.data = {p, ce}, .cb = state->cb, .ctx = state->ctx};
        ok64 o = MKDTInlineLexer(&ist);
        if (o != OK) return o;
    }

    // Emit trailing newline
    if (has_nl && state->cb) {
        u8cs nl = {ce, e};
        ok64 o = state->cb('S', nl, state->ctx);
        if (o != OK) return o;
    }

    return OK;
}

// --- Block-level lexer ---

ok64 MKDTLexer(MKDTstate *state) {
    sane($ok(state->data) && state != NULL);

    a_dup(u8c, scan, state->data);
    b8 in_fence = NO;
    int fence_len = 0;

    u8cs line = {};
    while (MDBLKu8csDrainLine(scan, line) == OK) {
        u8c *sol = (u8c *)line[0];   // start of line
        u8c *cur = (u8c *)line[1];   // end of line (past the '\n')

        if (in_fence) {
            if (MKDTFenceClose(line, fence_len))
                in_fence = NO;
            if (state->cb) {
                ok64 o = state->cb('H', line, state->ctx);
                if (o != OK) { state->data[0] = scan[0]; return o; }
            }
            continue;
        }

        int fl = MKDTFenceOpen(line);
        if (fl > 0) {
            in_fence = YES;
            fence_len = fl;
            if (state->cb) {
                ok64 o = state->cb('H', line, state->ctx);
                if (o != OK) { state->data[0] = scan[0]; return o; }
            }
        } else if (MKDTHRule(line)) {
            if (state->cb) {
                ok64 o = TOKSplitText('R', line, state->cb, state->ctx);
                if (o != OK) { state->data[0] = scan[0]; return o; }
            }
        } else if (MKDTRefDef(line)) {
            // Reference definition: emit as R (structural)
            if (state->cb) {
                ok64 o = TOKSplitText('R', line, state->cb, state->ctx);
                if (o != OK) { state->data[0] = scan[0]; return o; }
            }
        } else if (MKDTHeadingLevel(line) > 0) {
            ok64 o = MKDTEmitHeading(state, line);
            if (o != OK) { state->data[0] = scan[0]; return o; }
        } else {
            // Paragraph / list / blockquote / div — inline machine
            // Emit leading 4-char div markup blocks as R
            int depth = MKDTIndentDepth(line);

            // Check for block markers in the first non-indent 4-char group
            u8c *marker_end = NULL;
            MKDTLineMarker(line, depth, &marker_end);

            // Emit div markup (indents + marker) as R
            u8c *text_start = marker_end;
            if (text_start > sol && state->cb) {
                u8cs markup = {sol, text_start};
                ok64 o = TOKSplitText('R', markup, state->cb, state->ctx);
                if (o != OK) { state->data[0] = scan[0]; return o; }
            }

            // Run inline on the rest
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
