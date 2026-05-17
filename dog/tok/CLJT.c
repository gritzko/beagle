#include "CLJT.h"

#include "abc/PRO.h"

static const char *CLJT_KEYWORDS[] = {
    "def",      "defn",     "defmacro", "fn",       "if",
    "do",       "let",      "loop",     "recur",    "quote",
    "var",      "throw",    "try",      "catch",    "finally",
    "new",      "set!",     "ns",       "in-ns",    "import",
    "require",  "use",      "refer",
    NULL,
};

static b8 CLJTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = CLJT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 CLJTonComment(u8cs tok, CLJTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 CLJTonString(u8cs tok, CLJTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 CLJTonNumber(u8cs tok, CLJTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 CLJTonWord(u8cs tok, CLJTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = CLJTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 CLJTonPunct(u8cs tok, CLJTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 CLJTonSpace(u8cs tok, CLJTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
