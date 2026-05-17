#include "SCLT.h"

#include "abc/PRO.h"

static const char *SCLT_KEYWORDS[] = {
    "abstract",   "case",       "catch",      "class",
    "def",        "do",         "else",       "extends",
    "false",      "final",      "finally",    "for",
    "forSome",    "if",         "implicit",   "import",
    "lazy",       "match",      "new",        "null",
    "object",     "override",   "package",    "private",
    "protected",  "return",     "sealed",     "super",
    "this",       "throw",      "trait",      "true",
    "try",        "type",       "val",        "var",
    "while",      "with",       "yield",
    "given",      "using",      "enum",       "export",
    "then",
    NULL,
};

static b8 SCLTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = SCLT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 SCLTonComment(u8cs tok, SCLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 SCLTonString(u8cs tok, SCLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 SCLTonNumber(u8cs tok, SCLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 SCLTonAnnotation(u8cs tok, SCLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 SCLTonWord(u8cs tok, SCLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = SCLTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 SCLTonPunct(u8cs tok, SCLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 SCLTonSpace(u8cs tok, SCLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
