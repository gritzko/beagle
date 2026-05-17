#include "HTMT.h"

#include "abc/PRO.h"

ok64 HTMTonComment(u8cs tok, HTMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return TOKSplitText('D', tok, state->cb, state->ctx);
    done;
}

ok64 HTMTonString(u8cs tok, HTMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 HTMTonTag(u8cs tok, HTMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('R', tok, state->ctx);
    done;
}

ok64 HTMTonPunct(u8cs tok, HTMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 HTMTonText(u8cs tok, HTMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('S', tok, state->ctx);
    done;
}

ok64 HTMTonSpace(u8cs tok, HTMTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
