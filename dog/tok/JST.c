#include "JST.h"

#include "abc/PRO.h"

static const char *JST_KEYWORDS[] = {
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
    NULL,
};

static b8 JSTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = JST_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 JSTonComment(u8cs tok, JSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 JSTonString(u8cs tok, JSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 JSTonNumber(u8cs tok, JSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 JSTonWord(u8cs tok, JSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = JSTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 JSTonPunct(u8cs tok, JSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 JSTonSpace(u8cs tok, JSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
