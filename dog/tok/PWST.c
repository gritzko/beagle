#include "PWST.h"

#include "abc/PRO.h"

static const char *PWST_KEYWORDS[] = {
    "begin",    "break",    "catch",    "class",    "continue",
    "data",     "define",   "do",       "dynamicparam",
    "else",     "elseif",   "end",      "enum",     "exit",
    "filter",   "finally",  "for",      "foreach",  "from",
    "function", "hidden",   "if",       "in",       "inlinescript",
    "param",    "process",  "return",   "static",   "switch",
    "throw",    "trap",     "try",      "until",    "using",
    "var",      "while",    "workflow",
    NULL,
};

static b8 PWSTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = PWST_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 PWSTonComment(u8cs tok, PWSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 PWSTonString(u8cs tok, PWSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 PWSTonNumber(u8cs tok, PWSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 PWSTonVar(u8cs tok, PWSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('S', tok, state->ctx);
    done;
}

ok64 PWSTonWord(u8cs tok, PWSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = PWSTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 PWSTonPunct(u8cs tok, PWSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 PWSTonSpace(u8cs tok, PWSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
