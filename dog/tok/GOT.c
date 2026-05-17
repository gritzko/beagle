#include "GOT.h"

#include "abc/PRO.h"

static const char *GOT_KEYWORDS[] = {
    "break",       "case",     "chan",      "const",
    "continue",    "default",  "defer",     "else",
    "fallthrough", "for",      "func",      "go",
    "goto",        "if",       "import",    "interface",
    "map",         "package",  "range",     "return",
    "select",      "struct",   "switch",    "type",
    "var",
    "true",        "false",    "nil",
    "iota",
    NULL,
};

static b8 GOTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = GOT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 GOTonComment(u8cs tok, GOTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 GOTonString(u8cs tok, GOTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 GOTonNumber(u8cs tok, GOTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 GOTonWord(u8cs tok, GOTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = GOTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 GOTonPunct(u8cs tok, GOTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 GOTonSpace(u8cs tok, GOTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
