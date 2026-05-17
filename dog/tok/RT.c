#include "RT.h"

#include "abc/PRO.h"

static const char *RT_KEYWORDS[] = {
    "if",       "else",     "repeat",   "while",    "function",
    "for",      "in",       "next",     "break",    "TRUE",
    "FALSE",    "NULL",     "Inf",      "NaN",      "NA",
    "NA_integer_", "NA_real_", "NA_complex_", "NA_character_",
    "library",  "require",  "return",
    NULL,
};

static b8 RTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = RT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 RTonComment(u8cs tok, RTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 RTonString(u8cs tok, RTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 RTonNumber(u8cs tok, RTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 RTonWord(u8cs tok, RTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = RTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 RTonPunct(u8cs tok, RTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 RTonSpace(u8cs tok, RTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
