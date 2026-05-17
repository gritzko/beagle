#include "MDT.h"

#include "abc/PRO.h"

// Inline ragel lexer (MDT.rl.c, generated from MDT.c.rl)
ok64 MDTInlineLexer(MDTstate *state);

// --- Inline callbacks ---

ok64 MDTonEmph(u8cs tok, MDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 MDTonCode(u8cs tok, MDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 MDTonComment(u8cs tok, MDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 MDTonLink(u8cs tok, MDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 MDTonNumber(u8cs tok, MDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 MDTonWord(u8cs tok, MDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('S', tok, state->ctx);
    done;
}

ok64 MDTonPunct(u8cs tok, MDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 MDTonSpace(u8cs tok, MDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}

// --- Block-level helpers ---

// Check if line opens a fenced code block.
// Returns fence length (>=3) or 0. Sets *fc to fence char.
static int MDTFenceOpen(u8csc line, u8 *fc) {
    u8c *p = (u8c *)line[0];
    u8c *e = (u8c *)line[1];
    int spaces = 0;
    while (p < e && *p == ' ' && spaces < 3) { p++; spaces++; }
    if (p >= e) return 0;
    u8 ch = *p;
    if (ch != '`' && ch != '~') return 0;
    int count = 0;
    while (p < e && *p == ch) { p++; count++; }
    if (count < 3) return 0;
    if (ch == '`') {
        u8c *q = p;
        while (q < e) {
            if (*q == '`') return 0;
            q++;
        }
    }
    *fc = ch;
    return count;
}

// Check if line closes a fenced code block.
static b8 MDTFenceClose(u8csc line, u8 fc, int flen) {
    u8c *p = (u8c *)line[0];
    u8c *e = (u8c *)line[1];
    int spaces = 0;
    while (p < e && *p == ' ' && spaces < 3) { p++; spaces++; }
    if (p >= e || *p != fc) return NO;
    int count = 0;
    while (p < e && *p == fc) { p++; count++; }
    if (count < flen) return NO;
    while (p < e) {
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') return NO;
        p++;
    }
    return YES;
}

// Check ATX heading level (1-6), or 0.
static int MDTHeadingLevel(u8csc line) {
    u8c *p = (u8c *)line[0];
    u8c *e = (u8c *)line[1];
    int spaces = 0;
    while (p < e && *p == ' ' && spaces < 3) { p++; spaces++; }
    if (p >= e || *p != '#') return 0;
    int level = 0;
    while (p < e && *p == '#') { p++; level++; }
    if (level > 6) return 0;
    if (p < e && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
        return 0;
    return level;
}

// Check thematic break (---, ***, ___).
static b8 MDTThematicBreak(u8csc line) {
    u8c *p = (u8c *)line[0];
    u8c *e = (u8c *)line[1];
    int spaces = 0;
    while (p < e && *p == ' ' && spaces < 3) { p++; spaces++; }
    if (p >= e) return NO;
    u8 ch = *p;
    if (ch != '-' && ch != '*' && ch != '_') return NO;
    int count = 0;
    while (p < e) {
        if (*p == ch) count++;
        else if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
            return NO;
        p++;
    }
    return count >= 3;
}

// Wrapper callback: remap S → N for heading content
static ok64 MDTHeadingCb(u8 tag, u8cs tok, void *ctx) {
    MDTstate *st = (MDTstate *)ctx;
    if (tag == 'S') tag = 'N';
    return st->cb(tag, tok, st->ctx);
}

// Emit heading: prefix (#+ space) as R, content through inline.
static ok64 MDTEmitHeading(MDTstate *state, u8csc line) {
    u8c *p = (u8c *)line[0];
    u8c *e = (u8c *)line[1];

    // Skip leading spaces
    int spaces = 0;
    while (p < e && *p == ' ' && spaces < 3) { p++; spaces++; }

    // Consume # markers
    while (p < e && *p == '#') p++;

    // Consume space after #
    if (p < e && (*p == ' ' || *p == '\t')) p++;

    // Emit prefix as R via TOKSplitText
    u8cs prefix = {line[0], p};
    if (state->cb && !$empty(prefix)) {
        ok64 o = TOKSplitText('R', prefix, state->cb, state->ctx);
        if (o != OK) return o;
    }

    // Strip trailing newline for content
    u8c *ce = e;
    b8 has_nl = NO;
    if (ce > p && ce[-1] == '\n') { ce--; has_nl = YES; }

    // Run inline on heading content, remapping S → N
    if (p < ce) {
        MDTstate wrap = {.data = {NULL, NULL}, .cb = state->cb, .ctx = state->ctx};
        MDTstate ist = {.data = {p, ce}, .cb = MDTHeadingCb, .ctx = &wrap};
        ok64 o = MDTInlineLexer(&ist);
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

ok64 MDTLexer(MDTstate *state) {
    sane($ok(state->data) && state != NULL);

    u8c *cur = (u8c *)state->data[0];
    u8c *end = (u8c *)state->data[1];
    b8 in_fence = NO;
    int fence_len = 0;
    u8 fence_char = 0;

    while (cur < end) {
        // Find line boundary
        u8c *sol = cur;
        while (cur < end && *cur != '\n') cur++;
        if (cur < end) cur++;  // include newline
        u8cs line = {sol, cur};

        if (in_fence) {
            if (MDTFenceClose(line, fence_char, fence_len))
                in_fence = NO;
            if (state->cb) {
                ok64 o = state->cb('H', line, state->ctx);
                if (o != OK) { state->data[0] = cur; return o; }
            }
            continue;
        }

        u8 fc = 0;
        int fl = MDTFenceOpen(line, &fc);
        if (fl > 0) {
            in_fence = YES;
            fence_len = fl;
            fence_char = fc;
            if (state->cb) {
                ok64 o = state->cb('H', line, state->ctx);
                if (o != OK) { state->data[0] = cur; return o; }
            }
        } else if (MDTThematicBreak(line)) {
            if (state->cb) {
                ok64 o = TOKSplitText('R', line, state->cb, state->ctx);
                if (o != OK) { state->data[0] = cur; return o; }
            }
        } else if (MDTHeadingLevel(line) > 0) {
            ok64 o = MDTEmitHeading(state, line);
            if (o != OK) { state->data[0] = cur; return o; }
        } else {
            // Paragraph / list / blockquote — inline machine
            MDTstate ist = {
                .data = {line[0], line[1]},
                .cb = state->cb,
                .ctx = state->ctx,
            };
            ok64 o = MDTInlineLexer(&ist);
            if (o != OK) { state->data[0] = cur; return o; }
        }
    }

    state->data[0] = cur;
    return OK;
}
