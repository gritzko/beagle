#include "CSST.h"

#include "abc/PRO.h"

ok64 CSSTonComment(u8cs tok, CSSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 CSSTonString(u8cs tok, CSSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 CSSTonNumber(u8cs tok, CSSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 CSSTonAtRule(u8cs tok, CSSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('H', tok, state->ctx);
    done;
}

ok64 CSSTonWord(u8cs tok, CSSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('S', tok, state->ctx);
    done;
}

ok64 CSSTonPunct(u8cs tok, CSSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 CSSTonSpace(u8cs tok, CSSTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
