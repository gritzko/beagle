#include "PYT.h"

#include "abc/PRO.h"

static const char *PYT_KEYWORDS[] = {
    "False",    "None",     "True",     "and",      "as",
    "assert",   "async",    "await",    "break",    "class",
    "continue", "def",      "del",      "elif",     "else",
    "except",   "finally",  "for",      "from",     "global",
    "if",       "import",   "in",       "is",       "lambda",
    "nonlocal", "not",      "or",       "pass",     "raise",
    "return",   "try",      "while",    "with",     "yield",
    NULL,
};

static b8 PYTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = PYT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 PYTonComment(u8cs tok, PYTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 PYTonString(u8cs tok, PYTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 PYTonNumber(u8cs tok, PYTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 PYTonDecorator(u8cs tok, PYTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 PYTonWord(u8cs tok, PYTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = PYTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 PYTonPunct(u8cs tok, PYTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 PYTonSpace(u8cs tok, PYTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
