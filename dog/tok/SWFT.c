#include "SWFT.h"

#include "abc/PRO.h"

static const char *SWFT_KEYWORDS[] = {
    "associatedtype", "class",    "deinit",     "enum",
    "extension",  "fileprivate", "func",       "import",
    "init",       "inout",      "internal",   "let",
    "open",       "operator",   "private",    "protocol",
    "public",     "rethrows",   "static",     "struct",
    "subscript",  "typealias",  "var",
    "break",      "case",       "continue",   "default",
    "defer",      "do",         "else",       "fallthrough",
    "for",        "guard",      "if",         "in",
    "repeat",     "return",     "switch",     "where",
    "while",
    "Any",        "Self",       "as",         "catch",
    "false",      "is",         "nil",        "self",
    "super",      "throw",      "throws",     "true",
    "try",
    NULL,
};

static b8 SWFTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = SWFT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 SWFTonComment(u8cs tok, SWFTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 SWFTonString(u8cs tok, SWFTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 SWFTonNumber(u8cs tok, SWFTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 SWFTonWord(u8cs tok, SWFTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = SWFTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 SWFTonPunct(u8cs tok, SWFTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 SWFTonSpace(u8cs tok, SWFTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
