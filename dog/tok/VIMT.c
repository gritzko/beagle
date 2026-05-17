#include "VIMT.h"

#include "abc/PRO.h"

static const char *VIMT_KEYWORDS[] = {
    "if",       "else",     "elseif",   "endif",    "while",
    "endwhile", "for",      "endfor",   "in",       "do",
    "function", "endfunction", "return", "call",    "let",
    "set",      "unlet",    "execute",  "echo",     "echomsg",
    "echoerr",  "autocmd",  "command",  "map",      "noremap",
    "source",   "try",      "catch",    "finally",  "endtry",
    "throw",    "augroup",  "true",     "false",
    NULL,
};

static b8 VIMTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = VIMT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 VIMTonComment(u8cs tok, VIMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 VIMTonString(u8cs tok, VIMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 VIMTonNumber(u8cs tok, VIMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 VIMTonWord(u8cs tok, VIMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = VIMTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 VIMTonPunct(u8cs tok, VIMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 VIMTonSpace(u8cs tok, VIMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
