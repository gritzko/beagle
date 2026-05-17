#include "LLT.h"

#include "abc/PRO.h"

static const char *LLT_KEYWORDS[] = {
    // function definition/declaration
    "define", "declare",
    // terminators
    "ret", "br", "switch", "indirectbr", "invoke", "callbr",
    "resume", "unreachable",
    // memory
    "alloca", "load", "store", "fence", "cmpxchg", "atomicrmw",
    "getelementptr",
    // binary/bitwise
    "add", "sub", "mul", "udiv", "sdiv", "urem", "srem",
    "fadd", "fsub", "fmul", "fdiv", "frem",
    "and", "or", "xor", "shl", "lshr", "ashr",
    // comparison
    "icmp", "fcmp",
    // casts
    "trunc", "zext", "sext", "fptrunc", "fpext",
    "fptoui", "fptosi", "uitofp", "sitofp",
    "ptrtoint", "inttoptr", "bitcast", "addrspacecast",
    // other instructions
    "call", "select", "phi", "freeze", "fneg",
    "extractvalue", "insertvalue",
    "extractelement", "insertelement", "shufflevector",
    "landingpad", "catchswitch", "catchret", "catchpad",
    "cleanupret", "cleanuppad",
    // types
    "void", "ptr", "label", "metadata", "token", "opaque",
    "i1", "i8", "i16", "i32", "i64", "i128",
    "half", "float", "double", "fp128",
    // linkage/visibility
    "private", "internal", "external",
    "linkonce", "linkonce_odr", "weak", "weak_odr",
    "common", "appending", "hidden", "protected", "default",
    "dllimport", "dllexport",
    // attributes
    "nounwind", "readonly", "readnone", "writeonly",
    "alwaysinline", "noinline", "optnone", "optsize",
    "noreturn", "nofree", "nosync", "willreturn",
    "mustprogress", "uwtable", "cold", "convergent",
    "signext", "zeroext", "inreg", "byval", "sret",
    "noalias", "nocapture", "nonnull", "align",
    // calling conventions
    "ccc", "fastcc", "coldcc", "swiftcc", "tailcc",
    // constants/flags
    "null", "undef", "poison", "zeroinitializer",
    "true", "false", "to", "nsw", "nuw", "exact", "inbounds",
    "global", "constant", "unnamed_addr", "local_unnamed_addr",
    "source_filename", "target", "datalayout", "triple",
    "type", "comdat", "section", "attributes",
    // comparison predicates
    "eq", "ne", "slt", "sgt", "sle", "sge",
    "ult", "ugt", "ule", "uge",
    "oeq", "one", "olt", "ogt", "ole", "oge",
    "ord", "uno", "ueq", "une",
    NULL,
};

static b8 LLTIsKeyword(u8cs tok) {
    u64 len = u8csLen(tok);
    for (const char **kw = LLT_KEYWORDS; *kw; ++kw) {
        u64 kwlen = 0;
        const char *k = *kw;
        while (k[kwlen]) ++kwlen;
        if (kwlen != len) continue;
        if (__builtin_memcmp(tok[0], k, len) == 0) return YES;
    }
    return NO;
}

ok64 LLTonComment(u8cs tok, LLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 LLTonString(u8cs tok, LLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 LLTonNumber(u8cs tok, LLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 LLTonWord(u8cs tok, LLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = LLTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 LLTonPunct(u8cs tok, LLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 LLTonSpace(u8cs tok, LLTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
