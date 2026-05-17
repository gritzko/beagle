#include "CPPT.h"

#include "abc/PRO.h"

static const char *CPPT_KEYWORDS[] = {
    /* C keywords */
    "auto",     "break",    "case",     "char",     "const",
    "continue", "default",  "do",       "double",   "else",
    "enum",     "extern",   "float",    "for",      "goto",
    "if",       "inline",   "int",      "long",     "register",
    "return",   "restrict", "short",    "signed",   "sizeof",
    "static",   "struct",   "switch",   "typedef",  "union",
    "unsigned", "void",     "volatile", "while",
    /* C++ keywords */
    "alignas",  "alignof",  "bool",     "catch",    "char8_t",
    "char16_t", "char32_t", "class",    "co_await", "co_return",
    "co_yield", "concept",  "const_cast", "consteval", "constexpr",
    "constinit","decltype", "delete",   "dynamic_cast",
    "explicit", "export",   "false",    "friend",
    "mutable",  "namespace","new",      "noexcept", "nullptr",
    "operator", "private",  "protected","public",
    "reinterpret_cast","requires",
    "static_assert","static_cast",
    "template", "this",     "thread_local","throw",
    "true",     "try",      "typeid",   "typename",
    "using",    "virtual",  "wchar_t",
    "NULL",     "TRUE",     "FALSE",
    NULL,
};

static b8 CPPTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = CPPT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 CPPTonComment(u8cs tok, CPPTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 CPPTonString(u8cs tok, CPPTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 CPPTonNumber(u8cs tok, CPPTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 CPPTonPreproc(u8cs tok, CPPTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 CPPTonWord(u8cs tok, CPPTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = CPPTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 CPPTonPunct(u8cs tok, CPPTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 CPPTonSpace(u8cs tok, CPPTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
