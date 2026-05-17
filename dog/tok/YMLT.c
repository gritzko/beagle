#include "YMLT.h"

#include "abc/PRO.h"

static const char *YMLT_KEYWORDS[] = {
    "true", "false", "null", "yes", "no", "on", "off",
    "True", "False", "Null", "Yes", "No", "On", "Off",
    "TRUE", "FALSE", "NULL", "YES", "NO", "ON", "OFF",
    NULL,
};

static b8 YMLTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = YMLT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 YMLTonComment(u8cs tok, YMLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 YMLTonString(u8cs tok, YMLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 YMLTonNumber(u8cs tok, YMLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 YMLTonWord(u8cs tok, YMLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = YMLTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 YMLTonPunct(u8cs tok, YMLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 YMLTonSpace(u8cs tok, YMLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
