#include "CMKT.h"

#include "abc/PRO.h"

static const char *CMKT_KEYWORDS[] = {
    "if",       "elseif",   "else",     "endif",
    "foreach",  "endforeach",
    "while",    "endwhile",
    "function", "endfunction",
    "macro",    "endmacro",
    "set",      "unset",    "option",   "message",  "return",
    "include",  "find_package",
    "add_library",  "add_executable",
    "target_link_libraries",
    "project",  "cmake_minimum_required",
    NULL,
};

static b8 CMKTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = CMKT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 CMKTonComment(u8cs tok, CMKTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 CMKTonString(u8cs tok, CMKTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 CMKTonNumber(u8cs tok, CMKTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 CMKTonVar(u8cs tok, CMKTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('S', tok, state->ctx);
    done;
}

ok64 CMKTonWord(u8cs tok, CMKTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = CMKTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 CMKTonPunct(u8cs tok, CMKTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 CMKTonSpace(u8cs tok, CMKTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
