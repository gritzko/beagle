#include "SHT.h"

#include "abc/PRO.h"

static const char *SHT_KEYWORDS[] = {
    "if",       "then",     "elif",     "else",     "fi",
    "case",     "esac",     "for",      "while",    "until",
    "do",       "done",     "in",       "function", "select",
    "time",     "coproc",
    "local",    "declare",  "typeset",  "export",
    "readonly", "unset",
    "true",     "false",
    NULL,
};

static b8 SHTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = SHT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 SHTonComment(u8cs tok, SHTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 SHTonString(u8cs tok, SHTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 SHTonNumber(u8cs tok, SHTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 SHTonWord(u8cs tok, SHTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = SHTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 SHTonVar(u8cs tok, SHTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('S', tok, state->ctx);
    done;
}

ok64 SHTonPunct(u8cs tok, SHTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 SHTonSpace(u8cs tok, SHTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
