#include "NIMT.h"

#include "abc/PRO.h"

static const char *NIMT_KEYWORDS[] = {
    "addr",     "and",      "as",       "asm",      "bind",
    "block",    "break",    "case",     "cast",     "concept",
    "const",    "continue", "converter","defer",    "discard",
    "distinct", "div",      "do",       "elif",     "else",
    "end",      "enum",     "except",   "export",   "finally",
    "for",      "from",     "func",     "if",       "import",
    "in",       "include",  "interface","is",       "isnot",
    "iterator", "let",      "macro",    "method",   "mixin",
    "mod",      "nil",      "not",      "notin",    "object",
    "of",       "or",       "out",      "proc",     "ptr",
    "raise",    "ref",      "return",   "shl",      "shr",
    "static",   "template", "try",      "tuple",    "type",
    "using",    "var",      "when",     "while",    "xor",
    "yield",
    NULL,
};

static b8 NIMTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = NIMT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 NIMTonComment(u8cs tok, NIMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 NIMTonString(u8cs tok, NIMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 NIMTonNumber(u8cs tok, NIMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 NIMTonWord(u8cs tok, NIMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = NIMTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 NIMTonPunct(u8cs tok, NIMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 NIMTonSpace(u8cs tok, NIMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
