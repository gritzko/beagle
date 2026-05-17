#include "HST.h"

#include "abc/PRO.h"

static const char *HST_KEYWORDS[] = {
    "case",     "class",    "data",     "default",  "deriving",
    "do",       "else",     "forall",   "foreign",  "if",
    "import",   "in",       "infix",    "infixl",   "infixr",
    "instance", "let",      "module",   "newtype",  "of",
    "qualified","then",     "type",     "where",
    "True",     "False",
    NULL,
};

static b8 HSTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = HST_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 HSTonComment(u8cs tok, HSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 HSTonString(u8cs tok, HSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 HSTonNumber(u8cs tok, HSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 HSTonPragma(u8cs tok, HSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 HSTonWord(u8cs tok, HSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = HSTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 HSTonPunct(u8cs tok, HSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 HSTonSpace(u8cs tok, HSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
