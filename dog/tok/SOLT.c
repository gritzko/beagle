#include "SOLT.h"

#include "abc/PRO.h"

static const char *SOLT_KEYWORDS[] = {
    "pragma",   "solidity",  "import",   "from",     "as",
    "contract", "interface", "library",  "abstract", "is",
    "using",    "struct",    "enum",     "function", "modifier",
    "event",    "error",     "constructor",
    "mapping",  "address",   "bool",     "string",   "bytes",
    "int",      "uint",      "int8",     "uint8",
    "int16",    "uint16",    "int32",    "uint32",
    "int64",    "uint64",    "int128",   "uint128",
    "int256",   "uint256",   "bytes1",   "bytes32",
    "public",   "private",   "internal", "external",
    "pure",     "view",      "payable",
    "virtual",  "override",  "returns",
    "memory",   "storage",   "calldata",
    "if",       "else",      "for",      "while",    "do",
    "break",    "continue",  "return",
    "emit",     "revert",    "require",  "assembly",
    "true",     "false",     "new",      "delete",
    "this",     "super",
    NULL,
};

static b8 SOLTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = SOLT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 SOLTonComment(u8cs tok, SOLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 SOLTonString(u8cs tok, SOLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 SOLTonNumber(u8cs tok, SOLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 SOLTonWord(u8cs tok, SOLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = SOLTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 SOLTonPunct(u8cs tok, SOLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 SOLTonSpace(u8cs tok, SOLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
