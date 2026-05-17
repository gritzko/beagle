#include "CST.h"

#include "abc/PRO.h"

static const char *CST_KEYWORDS[] = {
    "abstract",  "as",        "base",      "bool",      "break",
    "byte",      "case",      "catch",     "char",      "checked",
    "class",     "const",     "continue",  "decimal",   "default",
    "delegate",  "do",        "double",    "else",      "enum",
    "event",     "explicit",  "extern",    "false",     "finally",
    "fixed",     "float",     "for",       "foreach",   "goto",
    "if",        "implicit",  "in",        "int",       "interface",
    "internal",  "is",        "lock",      "long",      "namespace",
    "new",       "null",      "object",    "operator",  "out",
    "override",  "params",    "private",   "protected", "public",
    "readonly",  "ref",       "return",    "sbyte",     "sealed",
    "short",     "sizeof",    "stackalloc","static",    "string",
    "struct",    "switch",    "this",      "throw",     "true",
    "try",       "typeof",    "uint",      "ulong",     "unchecked",
    "unsafe",    "ushort",    "using",     "virtual",   "void",
    "volatile",  "while",
    "var",       "dynamic",   "async",     "await",
    "yield",     "nameof",    "record",    "init",
    "required",  "global",
    NULL,
};

static b8 CSTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = CST_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 CSTonComment(u8cs tok, CSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 CSTonString(u8cs tok, CSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 CSTonNumber(u8cs tok, CSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 CSTonPreproc(u8cs tok, CSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 CSTonWord(u8cs tok, CSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = CSTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 CSTonPunct(u8cs tok, CSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 CSTonSpace(u8cs tok, CSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
