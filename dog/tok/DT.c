#include "DT.h"

#include "abc/PRO.h"

static const char *DT_KEYWORDS[] = {
    "abstract",   "alias",      "align",      "asm",
    "assert",     "auto",       "body",       "bool",
    "break",      "byte",       "case",       "cast",
    "catch",      "cdouble",    "cent",       "cfloat",
    "char",       "class",      "const",      "continue",
    "creal",      "dchar",      "debug",      "default",
    "delegate",   "delete",     "deprecated", "do",
    "double",     "else",       "enum",       "export",
    "extern",     "false",      "final",      "finally",
    "float",      "for",        "foreach",    "foreach_reverse",
    "function",   "goto",       "idouble",    "if",
    "ifloat",     "immutable",  "import",     "in",
    "inout",      "int",        "interface",  "invariant",
    "is",         "lazy",       "long",       "mixin",
    "module",     "new",        "nothrow",    "null",
    "out",        "override",   "package",    "pragma",
    "private",    "protected",  "public",     "pure",
    "real",       "ref",        "return",     "scope",
    "shared",     "short",      "static",     "struct",
    "super",      "switch",     "synchronized","template",
    "this",       "throw",      "true",       "try",
    "typeid",     "typeof",     "ubyte",      "ucent",
    "uint",       "ulong",      "union",      "unittest",
    "ushort",     "version",    "void",       "wchar",
    "while",      "with",
    NULL,
};

static b8 DTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = DT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 DTonComment(u8cs tok, DTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 DTonString(u8cs tok, DTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 DTonNumber(u8cs tok, DTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 DTonWord(u8cs tok, DTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = DTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 DTonPunct(u8cs tok, DTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 DTonSpace(u8cs tok, DTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
