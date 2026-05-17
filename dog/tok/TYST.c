#include "TYST.h"

#include "abc/PRO.h"

static const char *TYST_KEYWORDS[] = {
    "let",      "set",      "show",     "if",       "else",
    "for",      "while",    "import",   "include",  "in",
    "not",      "and",      "or",       "return",   "break",
    "continue", "none",     "auto",     "true",     "false",
    NULL,
};

static b8 TYSTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = TYST_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 TYSTonComment(u8cs tok, TYSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 TYSTonString(u8cs tok, TYSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 TYSTonNumber(u8cs tok, TYSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 TYSTonMath(u8cs tok, TYSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 TYSTonLabel(u8cs tok, TYSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('S', tok, state->ctx);
    done;
}

ok64 TYSTonPreproc(u8cs tok, TYSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 TYSTonWord(u8cs tok, TYSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = TYSTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 TYSTonPunct(u8cs tok, TYSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 TYSTonSpace(u8cs tok, TYSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
