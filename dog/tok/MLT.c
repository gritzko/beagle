#include "MLT.h"

#include "abc/PRO.h"

static const char *MLT_KEYWORDS[] = {
    "and",       "as",        "assert",    "begin",     "class",
    "constraint","do",        "done",      "downto",    "else",
    "end",       "exception", "external",  "false",     "for",
    "fun",       "function",  "functor",   "if",        "in",
    "include",   "inherit",   "initializer","lazy",     "let",
    "match",     "method",    "module",    "mutable",   "new",
    "nonrec",    "object",    "of",        "open",      "or",
    "private",   "rec",       "sig",       "struct",    "then",
    "to",        "true",      "try",       "type",      "val",
    "virtual",   "when",      "while",     "with",
    NULL,
};

static b8 MLTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = MLT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 MLTonComment(u8cs tok, MLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 MLTonString(u8cs tok, MLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 MLTonNumber(u8cs tok, MLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 MLTonWord(u8cs tok, MLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = MLTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 MLTonPunct(u8cs tok, MLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 MLTonSpace(u8cs tok, MLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
