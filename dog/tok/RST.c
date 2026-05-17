#include "RST.h"

#include "abc/PRO.h"

static const char *RST_KEYWORDS[] = {
    "as",       "async",    "await",    "break",    "const",
    "continue", "crate",    "dyn",      "else",     "enum",
    "extern",   "false",    "fn",       "for",      "if",
    "impl",     "in",       "let",      "loop",     "match",
    "mod",      "move",     "mut",      "pub",      "ref",
    "return",   "self",     "Self",     "static",   "struct",
    "super",    "trait",    "true",     "type",     "unsafe",
    "use",      "where",    "while",
    NULL,
};

static b8 RSTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = RST_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 RSTonComment(u8cs tok, RSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 RSTonString(u8cs tok, RSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 RSTonNumber(u8cs tok, RSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 RSTonAttr(u8cs tok, RSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 RSTonWord(u8cs tok, RSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = RSTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 RSTonPunct(u8cs tok, RSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 RSTonSpace(u8cs tok, RSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
