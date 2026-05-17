#include "LAXT.h"

#include "abc/PRO.h"

static const char *LAXT_KEYWORDS[] = {
    "begin", "end",
    "documentclass", "usepackage", "newcommand", "renewcommand",
    "section", "subsection", "subsubsection",
    "chapter", "part", "paragraph", "subparagraph",
    "textbf", "textit", "emph", "underline",
    "label", "ref", "cite", "bibliography",
    "include", "input",
    "if", "else", "fi", "ifx", "ifdef",
    "def", "let", "newenvironment", "renewenvironment",
    NULL,
};

static b8 LAXTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = LAXT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 LAXTonComment(u8cs tok, LAXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 LAXTonMath(u8cs tok, LAXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 LAXTonCommand(u8cs tok, LAXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('R', tok, state->ctx);
    done;
}

ok64 LAXTonNumber(u8cs tok, LAXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 LAXTonWord(u8cs tok, LAXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('S', tok, state->ctx);
    done;
}

ok64 LAXTonPunct(u8cs tok, LAXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 LAXTonSpace(u8cs tok, LAXTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
