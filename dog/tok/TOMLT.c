#include "TOMLT.h"

#include "abc/PRO.h"

static const char *TOMLT_KEYWORDS[] = {
    "true", "false",
    NULL,
};

static b8 TOMLTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = TOMLT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 TOMLTonComment(u8cs tok, TOMLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 TOMLTonString(u8cs tok, TOMLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 TOMLTonNumber(u8cs tok, TOMLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 TOMLTonHeader(u8cs tok, TOMLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('R', tok, state->cb, state->ctx);
    done;
}

ok64 TOMLTonWord(u8cs tok, TOMLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = TOMLTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 TOMLTonPunct(u8cs tok, TOMLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 TOMLTonSpace(u8cs tok, TOMLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
