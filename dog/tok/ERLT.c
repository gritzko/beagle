#include "ERLT.h"

#include "abc/PRO.h"

static const char *ERLT_KEYWORDS[] = {
    "after",    "and",      "andalso",  "band",     "begin",
    "bnot",     "bor",      "bsl",      "bsr",      "bxor",
    "case",     "catch",    "cond",     "div",      "else",
    "end",      "fun",      "if",       "let",      "not",
    "of",       "or",       "orelse",   "receive",  "rem",
    "try",      "when",     "xor",
    NULL,
};

static b8 ERLTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = ERLT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 ERLTonComment(u8cs tok, ERLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 ERLTonString(u8cs tok, ERLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 ERLTonNumber(u8cs tok, ERLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 ERLTonPreproc(u8cs tok, ERLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 ERLTonWord(u8cs tok, ERLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = ERLTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 ERLTonPunct(u8cs tok, ERLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 ERLTonSpace(u8cs tok, ERLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
