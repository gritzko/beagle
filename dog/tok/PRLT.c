#include "PRLT.h"

#include "abc/PRO.h"

static const char *PRLT_KEYWORDS[] = {
    "my",       "our",      "local",    "use",      "require",
    "no",       "strict",   "warnings", "sub",      "return",
    "if",       "elsif",    "else",     "unless",   "while",
    "until",    "for",      "foreach",  "do",       "next",
    "last",     "redo",     "goto",     "die",      "exit",
    "eval",     "BEGIN",    "END",      "print",    "say",
    NULL,
};

static b8 PRLTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = PRLT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 PRLTonComment(u8cs tok, PRLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 PRLTonString(u8cs tok, PRLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 PRLTonNumber(u8cs tok, PRLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 PRLTonWord(u8cs tok, PRLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = PRLTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 PRLTonPunct(u8cs tok, PRLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 PRLTonSpace(u8cs tok, PRLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
