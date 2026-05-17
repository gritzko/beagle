#include "BRCT.h"

// Extract the first source byte of token i.
static u8 brct_byte(u32csc toks, u8csc data, u64 i) {
    u32 lo = (i > 0) ? tok32Offset(toks[0][i - 1]) : 0;
    return data[0][lo];
}

static b8 brct_is_open(u8 ch) {
    return ch == '{' || ch == '(' || ch == '[';
}

static b8 brct_is_close(u8 ch) {
    return ch == '}' || ch == ')' || ch == ']';
}

static u8 brct_matching(u8 ch) {
    switch (ch) {
        case '{': return '}';
        case '}': return '{';
        case '(': return ')';
        case ')': return '(';
        case '[': return ']';
        case ']': return '[';
        default: return 0;
    }
}

static b8 brct_is_punct(u32 tok) { return tok32Tag(tok) == 'P'; }

i64 BRCTMatch(u32csc toks, u8csc data, u64 at) {
    u64 n = $len(toks);
    if (at >= n) return -1;
    if (!brct_is_punct(toks[0][at])) return -1;

    u8 ch = brct_byte(toks, data, at);
    u8 mate = brct_matching(ch);
    if (mate == 0) return -1;

    if (brct_is_open(ch)) {
        // scan forward
        i64 depth = 0;
        for (u64 i = at; i < n; i++) {
            if (!brct_is_punct(toks[0][i])) continue;
            u8 c = brct_byte(toks, data, i);
            if (c == ch) depth++;
            else if (c == mate) {
                depth--;
                if (depth == 0) return (i64)i;
            }
        }
    } else {
        // scan backward
        i64 depth = 0;
        for (i64 i = (i64)at; i >= 0; i--) {
            if (!brct_is_punct(toks[0][i])) continue;
            u8 c = brct_byte(toks, data, (u64)i);
            if (c == ch) depth++;
            else if (c == mate) {
                depth--;
                if (depth == 0) return i;
            }
        }
    }

    return -1;
}

ok64 BRCTInner(u64 *from, u64 *till, u32csc toks, u8csc data, u64 at) {
    // scan backward for the nearest unmatched open bracket
    i64 depth = 0;
    for (i64 i = (i64)at - 1; i >= 0; i--) {
        if (!brct_is_punct(toks[0][i])) continue;
        u8 c = brct_byte(toks, data, (u64)i);
        if (brct_is_close(c)) {
            depth++;
        } else if (brct_is_open(c)) {
            if (depth == 0) {
                i64 m = BRCTMatch(toks, data, (u64)i);
                if (m < 0 || (u64)m < at) return BRCTNONE;
                *from = (u64)i;
                *till = (u64)m;
                return OK;
            }
            depth--;
        }
    }
    return BRCTNONE;
}

ok64 BRCTOuter(u64 *from, u64 *till, u32csc toks, u8csc data, u64 at) {
    // find innermost first, then keep expanding
    u64 f, t;
    ok64 o = BRCTInner(&f, &t, toks, data, at);
    if (o != OK) return o;

    for (;;) {
        u64 f2, t2;
        o = BRCTInner(&f2, &t2, toks, data, f);
        if (o != OK) break;
        f = f2;
        t = t2;
    }

    *from = f;
    *till = t;
    return OK;
}

ok64 BRCTCheck(u32csc toks, u8csc data) {
    u8 stk[256];
    u64 top = 0;
    u64 n = $len(toks);

    for (u64 i = 0; i < n; i++) {
        if (!brct_is_punct(toks[0][i])) continue;
        u8 c = brct_byte(toks, data, i);
        if (brct_is_open(c)) {
            if (top >= 256) return BRCTBAD;
            stk[top++] = c;
        } else if (brct_is_close(c)) {
            if (top == 0) return BRCTBAD;
            u8 expected = brct_matching(c);
            if (stk[top - 1] != expected) return BRCTBAD;
            top--;
        }
    }

    return (top == 0) ? OK : BRCTBAD;
}

u64 BRCTDepth(u32csc toks, u8csc data, u64 at) {
    u64 depth = 0;
    u64 n = at < $len(toks) ? at : $len(toks);

    for (u64 i = 0; i < n; i++) {
        if (!brct_is_punct(toks[0][i])) continue;
        u8 c = brct_byte(toks, data, i);
        if (brct_is_open(c)) depth++;
        else if (brct_is_close(c) && depth > 0) depth--;
    }

    return depth;
}
