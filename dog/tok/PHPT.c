#include "PHPT.h"

#include "abc/PRO.h"

static const char *PHPT_KEYWORDS[] = {
    "abstract",  "and",       "array",     "as",        "break",
    "callable",  "case",      "catch",     "class",     "clone",
    "const",     "continue",  "declare",   "default",   "die",
    "do",        "echo",      "else",      "elseif",    "empty",
    "enddeclare","endfor",    "endforeach","endif",     "endswitch",
    "endwhile",  "enum",      "eval",      "exit",      "extends",
    "false",     "final",     "finally",   "fn",        "for",
    "foreach",   "function",  "global",    "goto",      "if",
    "implements","include",   "include_once","instanceof",
    "interface", "isset",     "list",      "match",     "namespace",
    "new",       "null",      "or",        "print",     "private",
    "protected", "public",    "readonly",  "require",   "require_once",
    "return",    "static",    "switch",    "throw",     "trait",
    "true",      "try",       "unset",     "use",       "var",
    "void",      "while",     "xor",       "yield",
    "self",      "parent",
    NULL,
};

static b8 PHPTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = PHPT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 PHPTonComment(u8cs tok, PHPTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 PHPTonString(u8cs tok, PHPTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 PHPTonNumber(u8cs tok, PHPTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 PHPTonPreproc(u8cs tok, PHPTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 PHPTonWord(u8cs tok, PHPTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = PHPTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 PHPTonPunct(u8cs tok, PHPTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 PHPTonSpace(u8cs tok, PHPTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
