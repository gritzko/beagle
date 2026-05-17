#include "TST.h"

#include "abc/PRO.h"

static const char *TST_KEYWORDS[] = {
    "break",    "case",     "catch",    "class",    "const",
    "continue", "debugger", "default",  "delete",   "do",
    "else",     "export",   "extends",  "false",    "finally",
    "for",      "function", "if",       "import",   "in",
    "instanceof", "let",    "new",      "null",     "of",
    "return",   "super",    "switch",   "this",     "throw",
    "true",     "try",      "typeof",   "var",      "void",
    "while",    "with",     "yield",
    "async",    "await",
    "undefined",
    "type",     "interface", "enum",    "implements",
    "declare",  "module",   "namespace", "abstract",
    "readonly", "keyof",    "infer",    "never",
    "unknown",  "any",      "is",       "asserts",
    NULL,
};

static b8 TSTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = TST_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 TSTonComment(u8cs tok, TSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 TSTonString(u8cs tok, TSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 TSTonNumber(u8cs tok, TSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 TSTonWord(u8cs tok, TSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = TSTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 TSTonPunct(u8cs tok, TSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 TSTonSpace(u8cs tok, TSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
