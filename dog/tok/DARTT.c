#include "DARTT.h"

#include "abc/PRO.h"

static const char *DARTT_KEYWORDS[] = {
    "abstract",   "as",         "assert",     "async",
    "await",      "break",      "case",       "catch",
    "class",      "const",      "continue",   "covariant",
    "default",    "deferred",   "do",         "dynamic",
    "else",       "enum",       "export",     "extends",
    "extension",  "external",   "factory",    "false",
    "final",      "finally",    "for",        "Function",
    "get",        "hide",       "if",         "implements",
    "import",     "in",         "interface",  "is",
    "late",       "library",    "mixin",      "new",
    "null",       "on",         "operator",   "part",
    "required",   "rethrow",    "return",     "sealed",
    "set",        "show",       "static",     "super",
    "switch",     "sync",       "this",       "throw",
    "true",       "try",        "typedef",    "var",
    "void",       "while",      "with",       "yield",
    NULL,
};

static b8 DARTTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = DARTT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 DARTTonComment(u8cs tok, DARTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 DARTTonString(u8cs tok, DARTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 DARTTonNumber(u8cs tok, DARTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 DARTTonAnnotation(u8cs tok, DARTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 DARTTonWord(u8cs tok, DARTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = DARTTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 DARTTonPunct(u8cs tok, DARTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 DARTTonSpace(u8cs tok, DARTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
