#include "FORT.h"

#include "abc/PRO.h"

static const char *FORT_KEYWORDS[] = {
    "program",  "end",      "subroutine", "function", "module",
    "use",      "implicit", "none",     "integer",  "real",
    "double",   "precision","character","logical",  "complex",
    "parameter","intent",   "in",       "out",      "inout",
    "allocatable","optional","pointer", "target",   "save",
    "data",     "dimension","if",       "then",     "else",
    "elseif",   "endif",    "do",       "enddo",    "while",
    "call",     "return",   "stop",     "exit",     "cycle",
    "where",    "forall",   "select",   "case",     "default",
    "type",     "class",    "print",    "write",    "read",
    "open",     "close",    "allocate", "deallocate","nullify",
    "interface","contains", "procedure","abstract", "extends",
    "public",   "private",  "protected",
    NULL,
};

static b8 FORTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = FORT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 FORTonComment(u8cs tok, FORTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 FORTonString(u8cs tok, FORTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 FORTonNumber(u8cs tok, FORTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 FORTonWord(u8cs tok, FORTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = FORTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 FORTonPunct(u8cs tok, FORTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 FORTonSpace(u8cs tok, FORTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
