#include "CT.h"

#include "abc/PRO.h"
#include "dog/tok/KEYW.h"

static u8cs CT_KEYWORDS[] = {
    u8slit("auto"),     u8slit("break"),    u8slit("case"),
    u8slit("char"),     u8slit("const"),    u8slit("continue"),
    u8slit("default"),  u8slit("do"),       u8slit("double"),
    u8slit("else"),     u8slit("enum"),     u8slit("extern"),
    u8slit("float"),    u8slit("for"),      u8slit("goto"),
    u8slit("if"),       u8slit("inline"),   u8slit("int"),
    u8slit("long"),     u8slit("register"), u8slit("return"),
    u8slit("restrict"), u8slit("short"),    u8slit("signed"),
    u8slit("sizeof"),   u8slit("static"),   u8slit("struct"),
    u8slit("switch"),   u8slit("typedef"),  u8slit("union"),
    u8slit("unsigned"), u8slit("void"),     u8slit("volatile"),
    u8slit("while"),
    u8slit("_Alignas"), u8slit("_Alignof"), u8slit("_Atomic"),
    u8slit("_Bool"),    u8slit("_Complex"), u8slit("_Generic"),
    u8slit("_Imaginary"), u8slit("_Noreturn"),
    u8slit("_Static_assert"), u8slit("_Thread_local"),
    u8slit("alignas"),  u8slit("alignof"),  u8slit("bool"),
    u8slit("static_assert"), u8slit("thread_local"),
    u8slit("typeof"),   u8slit("typeof_unqual"),
    u8slit("nullptr"),  u8slit("true"),     u8slit("false"),
    u8slit("constexpr"),
    u8slit("NULL"),     u8slit("TRUE"),     u8slit("FALSE"),
};
#define CT_NKW (sizeof(CT_KEYWORDS) / sizeof(CT_KEYWORDS[0]))

//  Lazy one-shot init.  `CTonWord` is the only caller; races are
//  benign (two threads would both populate the same table with the
//  same entries and converge).  No atomic needed at current use.
static keyw CT_KW;
static b8   CT_KW_INITED;

static b8 CTIsKeyword(u8cs tok) {
    if (!CT_KW_INITED) {
        if (KEYWOpen(&CT_KW, CT_KEYWORDS, CT_NKW) != OK) return NO;
        CT_KW_INITED = YES;
    }
    u8csc tc = {tok[0], tok[1]};
    return KEYWHas(&CT_KW, tc);
}

ok64 CTonComment(u8cs tok, CTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 CTonString(u8cs tok, CTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 CTonNumber(u8cs tok, CTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 CTonPreproc(u8cs tok, CTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 CTonWord(u8cs tok, CTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) {
        u8 tag = CTIsKeyword(tok) ? 'R' : 'S';
        return state->cb(tag, tok, state->ctx);
    }
    done;
}

ok64 CTonPunct(u8cs tok, CTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 CTonSpace(u8cs tok, CTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}

ok64 CTontok(u8cs tok, CTstate *state) { return OK; }
ok64 CTonRoot(u8cs tok, CTstate *state) { return OK; }
