#include "FSHT.h"

#include "abc/PRO.h"

static const char *FSHT_KEYWORDS[] = {
    "abstract", "and",      "as",       "assert",   "base",
    "begin",    "class",    "default",  "delegate", "do",
    "done",     "downcast", "downto",   "elif",     "else",
    "end",      "exception","extern",   "false",    "finally",
    "for",      "fun",      "function", "global",   "if",
    "in",       "inherit",  "inline",   "interface","internal",
    "lazy",     "let",      "match",    "member",   "module",
    "mutable",  "namespace","new",      "not",      "null",
    "of",       "open",     "or",       "override", "private",
    "public",   "rec",      "return",   "static",   "struct",
    "then",     "to",       "true",     "try",      "type",
    "upcast",   "use",      "val",      "void",     "when",
    "while",    "with",     "yield",
    NULL,
};

static b8 FSHTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = FSHT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 FSHTonComment(u8cs tok, FSHTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 FSHTonString(u8cs tok, FSHTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 FSHTonNumber(u8cs tok, FSHTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 FSHTonWord(u8cs tok, FSHTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = FSHTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 FSHTonPunct(u8cs tok, FSHTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 FSHTonSpace(u8cs tok, FSHTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
