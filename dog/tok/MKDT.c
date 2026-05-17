#include "MKDT.h"

#include "abc/PRO.h"

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

// --- Block-level helpers ---

// Check if line is a StrictMark code fence (3-4 backticks after div markup).
// Returns fence length (3 or 4) or 0.
static int MKDTFenceOpen(u8csc line) {
    u8c *p = (u8c *)line[0];
    u8c *e = (u8c *)line[1];
    // Skip 4-char indent blocks (div markup)
    while (p + 4 <= e && p[0] == ' ' && p[1] == ' ' &&
           p[2] == ' ' && p[3] == ' ')
        p += 4;
    if (p >= e || *p != '`') return 0;
    int count = 0;
    while (p < e && *p == '`') { p++; count++; }
    if (count < 3 || count > 4) return 0;
    return count;
}

// Check if line closes a fenced code block.
static b8 MKDTFenceClose(u8csc line, int flen) {
    u8c *p = (u8c *)line[0];
    u8c *e = (u8c *)line[1];
    // Skip 4-char indent blocks
    while (p + 4 <= e && p[0] == ' ' && p[1] == ' ' &&
           p[2] == ' ' && p[3] == ' ')
        p += 4;
    if (p >= e || *p != '`') return NO;
    int count = 0;
    while (p < e && *p == '`') { p++; count++; }
    if (count < flen) return NO;
    // Rest must be whitespace
    while (p < e) {
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') return NO;
        p++;
    }
    return YES;
}

// Check ATX heading level (1-4 only), with 4-char-wide markup.
// Returns level or 0.
static int MKDTHeadingLevel(u8csc line) {
    u8c *p = (u8c *)line[0];
    u8c *e = (u8c *)line[1];
    // Skip 4-char indent blocks (nesting)
    while (p + 4 <= e && p[0] == ' ' && p[1] == ' ' &&
           p[2] == ' ' && p[3] == ' ')
        p += 4;
    if (p >= e || *p != '#') return 0;
    int level = 0;
    while (p < e && *p == '#') { p++; level++; }
    if (level > 4) return 0;
    // Remaining chars in 4-char block must be spaces or start of text
    return level;
}

// Check horizontal rule: exactly "----"
static b8 MKDTHRule(u8csc line) {
    u8c *p = (u8c *)line[0];
    u8c *e = (u8c *)line[1];
    // Skip 4-char indent blocks
    while (p + 4 <= e && p[0] == ' ' && p[1] == ' ' &&
           p[2] == ' ' && p[3] == ' ')
        p += 4;
    if (p + 4 > e) return NO;
    if (p[0] != '-' || p[1] != '-' || p[2] != '-' || p[3] != '-') return NO;
    p += 4;
    // Rest must be whitespace/newline
    while (p < e) {
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') return NO;
        p++;
    }
    return YES;
}

// Check reference definition: [x]: ...
static b8 MKDTRefDef(u8csc line) {
    u8c *p = (u8c *)line[0];
    u8c *e = (u8c *)line[1];
    // Skip 4-char indent blocks
    while (p + 4 <= e && p[0] == ' ' && p[1] == ' ' &&
           p[2] == ' ' && p[3] == ' ')
        p += 4;
    if (p + 4 > e) return NO;
    if (p[0] != '[') return NO;
    u8 c = p[1];
    b8 alnum = (c >= '0' && c <= '9') ||
               (c >= 'A' && c <= 'Z') ||
               (c >= 'a' && c <= 'z');
    if (!alnum) return NO;
    if (p[2] != ']' || p[3] != ':') return NO;
    return YES;
}

// Count leading 4-space indent blocks (div markup depth).
static int MKDTIndentDepth(u8csc line) {
    u8c *p = (u8c *)line[0];
    u8c *e = (u8c *)line[1];
    int depth = 0;
    while (p + 4 <= e && p[0] == ' ' && p[1] == ' ' &&
           p[2] == ' ' && p[3] == ' ') {
        p += 4;
        depth++;
    }
    return depth;
}

// Emit heading: prefix (div markup + #+ space) as R, content through inline.
static ok64 MKDTEmitHeading(MKDTstate *state, u8csc line) {
    u8c *p = (u8c *)line[0];
    u8c *e = (u8c *)line[1];

    // Skip 4-char indent blocks
    while (p + 4 <= e && p[0] == ' ' && p[1] == ' ' &&
           p[2] == ' ' && p[3] == ' ')
        p += 4;

    // Consume # markers
    while (p < e && *p == '#') p++;

    // Consume trailing spaces in 4-char block
    while (p < e && *p == ' ') p++;

    // Emit prefix as R
    u8cs prefix = {line[0], p};
    if (state->cb && !$empty(prefix)) {
        ok64 o = TOKSplitText('R', prefix, state->cb, state->ctx);
        if (o != OK) return o;
    }

    // Strip trailing newline for content
    u8c *ce = e;
    b8 has_nl = NO;
    if (ce > p && ce[-1] == '\n') { ce--; has_nl = YES; }

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

    u8c *cur = (u8c *)state->data[0];
    u8c *end = (u8c *)state->data[1];
    b8 in_fence = NO;
    int fence_len = 0;

    while (cur < end) {
        // Find line boundary
        u8c *sol = cur;
        while (cur < end && *cur != '\n') cur++;
        if (cur < end) cur++;  // include newline
        u8cs line = {sol, cur};

        if (in_fence) {
            if (MKDTFenceClose(line, fence_len))
                in_fence = NO;
            if (state->cb) {
                ok64 o = state->cb('H', line, state->ctx);
                if (o != OK) { state->data[0] = cur; return o; }
            }
            continue;
        }

        int fl = MKDTFenceOpen(line);
        if (fl > 0) {
            in_fence = YES;
            fence_len = fl;
            if (state->cb) {
                ok64 o = state->cb('H', line, state->ctx);
                if (o != OK) { state->data[0] = cur; return o; }
            }
        } else if (MKDTHRule(line)) {
            if (state->cb) {
                ok64 o = TOKSplitText('R', line, state->cb, state->ctx);
                if (o != OK) { state->data[0] = cur; return o; }
            }
        } else if (MKDTRefDef(line)) {
            // Reference definition: emit as R (structural)
            if (state->cb) {
                ok64 o = TOKSplitText('R', line, state->cb, state->ctx);
                if (o != OK) { state->data[0] = cur; return o; }
            }
        } else if (MKDTHeadingLevel(line) > 0) {
            ok64 o = MKDTEmitHeading(state, line);
            if (o != OK) { state->data[0] = cur; return o; }
        } else {
            // Paragraph / list / blockquote / div — inline machine
            // Emit leading 4-char div markup blocks as R
            int depth = MKDTIndentDepth(line);
            u8c *content = sol + depth * 4;

            // Check for block markers in the first non-indent 4-char group
            b8 has_marker = NO;
            u8c *marker_end = content;
            if (content + 4 <= cur) {
                // Blockquote: >___
                if (content[0] == '>' ||
                    (content[0] == ' ' && content[1] == '>') ||
                    (content[0] == ' ' && content[1] == ' ' && content[2] == '>') ||
                    (content[0] == ' ' && content[1] == ' ' && content[2] == ' ' && content[3] == '>')) {
                    has_marker = YES;
                    marker_end = content + 4;
                }
                // Unordered list: -___
                else if (content[0] == '-' ||
                         (content[0] == ' ' && content[1] == '-') ||
                         (content[0] == ' ' && content[1] == ' ' && content[2] == '-') ||
                         (content[0] == ' ' && content[1] == ' ' && content[2] == ' ' && content[3] == '-')) {
                    has_marker = YES;
                    marker_end = content + 4;
                }
                // Ordered list: N. or NN. etc
                else if ((content[0] >= '0' && content[0] <= '9') ||
                         (content[0] == ' ' && content[1] >= '0' && content[1] <= '9')) {
                    // Scan for digits then dot
                    u8c *q = content;
                    while (q < content + 4 && *q == ' ') q++;
                    while (q < content + 4 && *q >= '0' && *q <= '9') q++;
                    if (q < content + 4 && *q == '.') {
                        has_marker = YES;
                        marker_end = content + 4;
                    }
                }
                // TODO: [ ] [x] [X]
                else if (content + 4 <= cur &&
                         content[0] == '[' &&
                         (content[1] == ' ' || content[1] == 'x' || content[1] == 'X') &&
                         content[2] == ']' && content[3] == ' ') {
                    has_marker = YES;
                    marker_end = content + 4;
                }
            }

            // Emit div markup (indents + marker) as R
            u8c *text_start = has_marker ? marker_end : content;
            if (text_start > sol && state->cb) {
                u8cs markup = {sol, text_start};
                ok64 o = TOKSplitText('R', markup, state->cb, state->ctx);
                if (o != OK) { state->data[0] = cur; return o; }
            }

            // Run inline on the rest
            if (text_start < cur) {
                MKDTstate ist = {
                    .data = {text_start, cur},
                    .cb = state->cb,
                    .ctx = state->ctx,
                };
                ok64 o = MKDTInlineLexer(&ist);
                if (o != OK) { state->data[0] = cur; return o; }
            }
        }
    }

    state->data[0] = cur;
    return OK;
}
