#include "HCLT.h"

#include "abc/PRO.h"

static const char *HCLT_KEYWORDS[] = {
    "variable", "resource", "data", "module", "output",
    "locals", "provider", "terraform", "backend",
    "required_providers", "for_each", "count",
    "depends_on", "lifecycle", "source", "version",
    "type", "default", "description", "sensitive", "nullable",
    "dynamic", "content", "each",
    "if", "else", "endif", "for", "in",
    "true", "false", "null",
    NULL,
};

static b8 HCLTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = HCLT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 HCLTonComment(u8cs tok, HCLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 HCLTonString(u8cs tok, HCLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 HCLTonNumber(u8cs tok, HCLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 HCLTonWord(u8cs tok, HCLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = HCLTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 HCLTonPunct(u8cs tok, HCLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 HCLTonSpace(u8cs tok, HCLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
