#include "NIXT.h"

#include "abc/PRO.h"

static const char *NIXT_KEYWORDS[] = {
    "assert",   "builtins", "else",     "if",       "in",
    "inherit",  "let",      "or",       "rec",      "then",
    "with",
    NULL,
};

static b8 NIXTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = NIXT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 NIXTonComment(u8cs tok, NIXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 NIXTonString(u8cs tok, NIXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 NIXTonNumber(u8cs tok, NIXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 NIXTonWord(u8cs tok, NIXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = NIXTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 NIXTonPunct(u8cs tok, NIXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 NIXTonSpace(u8cs tok, NIXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
