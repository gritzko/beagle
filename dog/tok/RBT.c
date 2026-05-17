#include "RBT.h"

#include "abc/PRO.h"

static const char *RBT_KEYWORDS[] = {
    "alias",    "and",      "begin",    "break",    "case",
    "class",    "def",      "defined?", "do",       "else",
    "elsif",    "end",      "ensure",   "false",    "for",
    "if",       "in",       "module",   "next",     "nil",
    "not",      "or",       "redo",     "rescue",   "retry",
    "return",   "self",     "super",    "then",     "true",
    "undef",    "unless",   "until",    "when",     "while",
    "yield",
    "BEGIN",    "END",
    "__FILE__", "__LINE__", "__ENCODING__",
    NULL,
};

static b8 RBTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = RBT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 RBTonComment(u8cs tok, RBTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 RBTonString(u8cs tok, RBTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 RBTonNumber(u8cs tok, RBTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 RBTonWord(u8cs tok, RBTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = RBTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 RBTonPunct(u8cs tok, RBTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 RBTonSpace(u8cs tok, RBTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
