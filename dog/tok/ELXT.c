#include "ELXT.h"

#include "abc/PRO.h"

static const char *ELXT_KEYWORDS[] = {
    "after",    "alias",    "and",      "case",     "catch",
    "cond",     "def",      "defp",     "defmacro", "defmacrop",
    "defmodule","defprotocol","defimpl", "defstruct","defdelegate",
    "defguard", "defguardp","defoverridable",
    "do",       "else",     "end",      "false",    "fn",
    "for",      "if",       "import",   "in",       "nil",
    "not",      "or",       "quote",    "raise",    "receive",
    "require",  "rescue",   "true",     "try",      "unless",
    "unquote",  "unquote_splicing",     "use",      "when",
    "with",
    NULL,
};

static b8 ELXTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = ELXT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 ELXTonComment(u8cs tok, ELXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 ELXTonString(u8cs tok, ELXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 ELXTonNumber(u8cs tok, ELXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 ELXTonDecorator(u8cs tok, ELXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 ELXTonWord(u8cs tok, ELXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = ELXTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 ELXTonPunct(u8cs tok, ELXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 ELXTonSpace(u8cs tok, ELXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
