#include "ZIGT.h"

#include "abc/PRO.h"

static const char *ZIGT_KEYWORDS[] = {
    "addrspace",  "align",      "allowzero",  "and",
    "anyerror",   "anyframe",   "anytype",    "asm",
    "async",      "await",      "break",      "callconv",
    "catch",      "comptime",   "const",      "continue",
    "defer",      "else",       "enum",       "errdefer",
    "error",      "export",     "extern",     "false",
    "fn",         "for",        "if",         "inline",
    "linksection","noalias",    "nosuspend",  "null",
    "orelse",     "or",         "packed",     "pub",
    "resume",     "return",     "struct",     "suspend",
    "switch",     "test",       "threadlocal","true",
    "try",        "undefined",  "union",      "unreachable",
    "usingnamespace", "var",    "volatile",   "while",
    NULL,
};

static b8 ZIGTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = ZIGT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 ZIGTonComment(u8cs tok, ZIGTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 ZIGTonString(u8cs tok, ZIGTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 ZIGTonNumber(u8cs tok, ZIGTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 ZIGTonWord(u8cs tok, ZIGTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = ZIGTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 ZIGTonPunct(u8cs tok, ZIGTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 ZIGTonSpace(u8cs tok, ZIGTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
