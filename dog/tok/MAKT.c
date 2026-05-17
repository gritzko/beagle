#include "MAKT.h"

#include "abc/PRO.h"

static const char *MAKT_KEYWORDS[] = {
    ".PHONY",   ".DEFAULT", ".PRECIOUS",".INTERMEDIATE",
    ".SECONDARY",".SUFFIXES",".DELETE_ON_ERROR",
    "include",  "-include", "sinclude",
    "define",   "endef",
    "ifdef",    "ifndef",   "ifeq",     "ifneq",
    "else",     "endif",
    "export",   "unexport", "override",
    NULL,
};

static b8 MAKTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = MAKT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 MAKTonComment(u8cs tok, MAKTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 MAKTonString(u8cs tok, MAKTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 MAKTonNumber(u8cs tok, MAKTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 MAKTonVar(u8cs tok, MAKTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('S', tok, state->ctx);
    done;
}

ok64 MAKTonWord(u8cs tok, MAKTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = MAKTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 MAKTonPunct(u8cs tok, MAKTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 MAKTonSpace(u8cs tok, MAKTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
