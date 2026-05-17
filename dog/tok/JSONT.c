#include "JSONT.h"

#include "abc/PRO.h"

ok64 JSONTonString(u8cs tok, JSONTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('G', tok, state->ctx);
    done;
}

ok64 JSONTonNumber(u8cs tok, JSONTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('L', tok, state->ctx);
    done;
}

ok64 JSONTonKeyword(u8cs tok, JSONTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('R', tok, state->ctx);
    done;
}

ok64 JSONTonPunct(u8cs tok, JSONTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('P', tok, state->ctx);
    done;
}

ok64 JSONTonSpace(u8cs tok, JSONTstate *state) {
    sane($ok(tok) && state != NULL);
    if (state->cb) return state->cb('W', tok, state->ctx);
    done;
}
