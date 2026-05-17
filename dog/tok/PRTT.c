#include "PRTT.h"

#include "abc/PRO.h"

static const char *PRTT_KEYWORDS[] = {
    "syntax", "import", "weak", "public", "package",
    "option", "message", "enum", "service", "rpc",
    "returns", "stream", "map", "oneof", "reserved",
    "extensions", "to", "repeated", "optional", "required",
    "group", "extend",
    "double", "float", "int32", "int64",
    "uint32", "uint64", "sint32", "sint64",
    "fixed32", "fixed64", "sfixed32", "sfixed64",
    "bool", "string", "bytes",
    "true", "false",
    NULL,
};

static b8 PRTTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = PRTT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 PRTTonComment(u8cs tok, PRTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 PRTTonString(u8cs tok, PRTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 PRTTonNumber(u8cs tok, PRTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 PRTTonWord(u8cs tok, PRTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = PRTTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 PRTTonPunct(u8cs tok, PRTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 PRTTonSpace(u8cs tok, PRTTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
