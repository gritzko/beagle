#include "JAT.h"

#include "abc/PRO.h"

static const char *JAT_KEYWORDS[] = {
    "abstract",   "assert",     "boolean",    "break",
    "byte",       "case",       "catch",      "char",
    "class",      "const",      "continue",   "default",
    "do",         "double",     "else",       "enum",
    "extends",    "final",      "finally",    "float",
    "for",        "goto",       "if",         "implements",
    "import",     "instanceof", "int",        "interface",
    "long",       "native",     "new",        "package",
    "private",    "protected",  "public",     "return",
    "short",      "static",     "strictfp",   "super",
    "switch",     "synchronized","this",      "throw",
    "throws",     "transient",  "try",        "void",
    "volatile",   "while",
    "var",        "yield",      "sealed",     "permits",
    "record",
    "true",       "false",      "null",
    NULL,
};

static b8 JATIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = JAT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 JATonComment(u8cs tok, JATstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 JATonString(u8cs tok, JATstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 JATonNumber(u8cs tok, JATstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 JATonAnnotation(u8cs tok, JATstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 JATonWord(u8cs tok, JATstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = JATIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 JATonPunct(u8cs tok, JATstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 JATonSpace(u8cs tok, JATstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
