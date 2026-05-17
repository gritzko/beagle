#include "ODNT.h"

#include "abc/PRO.h"

static const char *ODNT_KEYWORDS[] = {
    "break",    "case",     "cast",     "continue", "defer",
    "distinct", "do",       "dynamic",  "else",     "enum",
    "fallthrough","for",    "foreign",  "if",       "import",
    "in",       "map",      "not_in",   "or_else",  "or_return",
    "package",  "proc",     "return",   "struct",   "switch",
    "transmute","typeid",   "union",    "using",    "when",
    "where",
    NULL,
};

static b8 ODNTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = ODNT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 ODNTonComment(u8cs tok, ODNTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 ODNTonString(u8cs tok, ODNTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 ODNTonNumber(u8cs tok, ODNTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 ODNTonWord(u8cs tok, ODNTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = ODNTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 ODNTonPunct(u8cs tok, ODNTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 ODNTonSpace(u8cs tok, ODNTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
