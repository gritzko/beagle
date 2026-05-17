#include "JLT.h"

#include "abc/PRO.h"

static const char *JLT_KEYWORDS[] = {
    "baremodule","begin",    "break",    "catch",    "const",
    "continue",  "do",       "else",     "elseif",   "end",
    "export",    "false",    "finally",  "for",      "function",
    "global",    "if",       "import",   "in",       "let",
    "local",     "macro",    "module",   "mutable",  "nothing",
    "quote",     "return",   "struct",   "true",     "try",
    "using",     "where",    "while",
    "abstract",  "primitive","type",
    NULL,
};

static b8 JLTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = JLT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 JLTonComment(u8cs tok, JLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 JLTonString(u8cs tok, JLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 JLTonNumber(u8cs tok, JLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 JLTonWord(u8cs tok, JLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = JLTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 JLTonPunct(u8cs tok, JLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 JLTonSpace(u8cs tok, JLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
