#include "KTT.h"

#include "abc/PRO.h"

static const char *KTT_KEYWORDS[] = {
    "abstract",   "annotation", "as",         "break",
    "by",         "catch",      "class",      "companion",
    "const",      "constructor","continue",   "crossinline",
    "data",       "delegate",   "do",         "dynamic",
    "else",       "enum",       "expect",     "external",
    "false",      "final",      "finally",    "for",
    "fun",        "get",        "if",         "import",
    "in",         "infix",      "init",       "inline",
    "inner",      "interface",  "internal",   "is",
    "lateinit",   "noinline",   "null",       "object",
    "open",       "operator",   "out",        "override",
    "package",    "private",    "protected",  "public",
    "reified",    "return",     "sealed",     "set",
    "super",      "suspend",    "this",       "throw",
    "true",       "try",        "typealias",  "typeof",
    "val",        "var",        "vararg",     "when",
    "where",      "while",      "yield",
    NULL,
};

static b8 KTTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = KTT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 KTTonComment(u8cs tok, KTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 KTTonString(u8cs tok, KTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 KTTonNumber(u8cs tok, KTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 KTTonAnnotation(u8cs tok, KTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 KTTonWord(u8cs tok, KTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = KTTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 KTTonPunct(u8cs tok, KTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 KTTonSpace(u8cs tok, KTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
