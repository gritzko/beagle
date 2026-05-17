#include "GQLT.h"

#include "abc/PRO.h"

static const char *GQLT_KEYWORDS[] = {
    "query", "mutation", "subscription", "fragment", "on",
    "type", "interface", "union", "enum", "scalar",
    "implements", "extends", "input", "directive",
    "schema", "extend", "true", "false", "null",
    NULL,
};

static b8 GQLTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = GQLT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 GQLTonComment(u8cs tok, GQLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 GQLTonString(u8cs tok, GQLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 GQLTonNumber(u8cs tok, GQLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 GQLTonWord(u8cs tok, GQLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = GQLTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 GQLTonPunct(u8cs tok, GQLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 GQLTonSpace(u8cs tok, GQLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
