#include "GLMT.h"

#include "abc/PRO.h"

static const char *GLMT_KEYWORDS[] = {
    "as",       "assert",   "case",     "const",    "external",
    "fn",       "if",       "import",   "let",      "opaque",
    "pub",      "todo",     "try",      "type",     "use",
    NULL,
};

static b8 GLMTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = GLMT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 GLMTonComment(u8cs tok, GLMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 GLMTonString(u8cs tok, GLMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 GLMTonNumber(u8cs tok, GLMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 GLMTonWord(u8cs tok, GLMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = GLMTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 GLMTonPunct(u8cs tok, GLMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 GLMTonSpace(u8cs tok, GLMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
