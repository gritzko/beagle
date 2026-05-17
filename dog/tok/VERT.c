#include "VERT.h"

#include "abc/PRO.h"

static const char *VERT_KEYWORDS[] = {
    "always",    "always_comb","always_ff","always_latch",
    "and",       "assign",    "automatic", "begin",
    "bit",       "buf",       "bufif0",    "bufif1",
    "byte",      "case",      "casex",     "casez",
    "cell",      "chandle",   "class",     "clocking",
    "cmos",      "config",    "const",     "constraint",
    "context",   "continue",  "cover",     "covergroup",
    "coverpoint","cross",     "deassign",  "default",
    "defparam",  "disable",   "dist",      "do",
    "edge",      "else",      "end",       "endcase",
    "endclass",  "endclocking","endconfig","endfunction",
    "endgenerate","endgroup", "endinterface","endmodule",
    "endpackage","endprimitive","endprogram","endproperty",
    "endsequence","endspecify","endtable","endtask",
    "enum",      "event",     "expect",    "export",
    "extends",   "extern",    "final",     "first_match",
    "for",       "force",     "foreach",   "forever",
    "fork",      "forkjoin",  "function",  "generate",
    "genvar",    "highz0",    "highz1",    "if",
    "iff",       "ifnone",    "ignore_bins","illegal_bins",
    "import",    "incdir",    "include",   "initial",
    "inout",     "input",     "inside",    "instance",
    "int",       "integer",   "interface", "intersect",
    "join",      "join_any",  "join_none", "large",
    "let",       "liblist",   "library",   "local",
    "localparam","logic",     "longint",   "macromodule",
    "matches",   "medium",    "modport",   "module",
    "nand",      "negedge",   "new",       "nexttime",
    "nmos",      "nor",       "noshowcancelled","not",
    "notif0",    "notif1",    "null",      "or",
    "output",    "package",   "packed",    "parameter",
    "pmos",      "posedge",   "primitive", "priority",
    "program",   "property",  "protected", "pull0",
    "pull1",     "pulldown",  "pullup",    "pulsestyle_ondetect",
    "pulsestyle_onevent","pure","rand",    "randc",
    "randcase",  "randsequence","rcmos",   "real",
    "realtime",  "ref",       "reg",       "reject_on",
    "release",   "repeat",    "return",    "rnmos",
    "rpmos",     "rtran",     "rtranif0",  "rtranif1",
    "s_always",  "s_eventually","s_nexttime","s_until",
    "s_until_with","scalared","sequence",  "shortint",
    "shortreal", "showcancelled","signed", "small",
    "solve",     "specify",   "specparam", "static",
    "string",    "strong",    "strong0",   "strong1",
    "struct",    "super",     "supply0",   "supply1",
    "sync_accept_on","sync_reject_on","table","tagged",
    "task",      "this",      "throughout","time",
    "timeprecision","timeunit","tran",     "tranif0",
    "tranif1",   "tri",       "tri0",      "tri1",
    "triand",    "trior",     "trireg",    "type",
    "typedef",   "union",     "unique",    "unique0",
    "unsigned",  "until",     "until_with","untyped",
    "use",       "uwire",     "var",       "vectored",
    "virtual",   "void",      "wait",      "wait_order",
    "wand",      "weak",      "weak0",     "weak1",
    "while",     "wildcard",  "wire",      "with",
    "within",    "wor",       "xnor",      "xor",
    NULL,
};

static b8 VERTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = VERT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 VERTonComment(u8cs tok, VERTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 VERTonString(u8cs tok, VERTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 VERTonNumber(u8cs tok, VERTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 VERTonPreproc(u8cs tok, VERTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 VERTonWord(u8cs tok, VERTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = VERTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 VERTonPunct(u8cs tok, VERTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 VERTonSpace(u8cs tok, VERTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
