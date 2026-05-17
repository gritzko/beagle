#include "LUAT.h"

#include "abc/PRO.h"

static const char *LUAT_KEYWORDS[] = {
    "and",      "break",    "do",       "else",     "elseif",
    "end",      "false",    "for",      "function", "goto",
    "if",       "in",       "local",    "nil",      "not",
    "or",       "repeat",   "return",   "then",     "true",
    "until",    "while",
    NULL,
};

static b8 LUATIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = LUAT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 LUATonComment(u8cs tok, LUATstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 LUATonString(u8cs tok, LUATstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 LUATonNumber(u8cs tok, LUATstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 LUATonWord(u8cs tok, LUATstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = LUATIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 LUATonPunct(u8cs tok, LUATstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 LUATonSpace(u8cs tok, LUATstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
