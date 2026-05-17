#include "AGDT.h"

#include "abc/PRO.h"

static const char *AGDT_KEYWORDS[] = {
    "abstract",  "constructor","data",     "do",       "eta-equality",
    "field",     "forall",    "hiding",   "import",   "in",
    "inductive", "infix",     "infixl",   "infixr",   "instance",
    "interleaved","let",      "macro",    "module",   "mutual",
    "no-eta-equality","open", "overlap",  "pattern",  "postulate",
    "primitive", "private",   "public",   "quote",    "quoteTerm",
    "record",    "renaming",  "rewrite",  "Set",      "syntax",
    "tactic",    "to",        "unquote",  "unquoteDecl","unquoteDef",
    "using",     "variable",  "where",    "with",
    NULL,
};

static b8 AGDTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = AGDT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 AGDTonComment(u8cs tok, AGDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 AGDTonString(u8cs tok, AGDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 AGDTonNumber(u8cs tok, AGDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 AGDTonPragma(u8cs tok, AGDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 AGDTonWord(u8cs tok, AGDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = AGDTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 AGDTonPunct(u8cs tok, AGDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 AGDTonSpace(u8cs tok, AGDTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
