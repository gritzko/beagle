#include "abc/INT.h"
#include "abc/PRO.h"
#include "FREE.h"

%%{

machine FREE;

alphtype unsigned char;

any8        = (0x00..0xff);
ws_h        = [ \t\r\f\v];
dgt         = [0-9];
xdgt        = [0-9a-fA-F];
ascii_alpha = [a-zA-Z_];
ascii_alnum = [a-zA-Z_0-9];
uc          = [A-Z];
ucnum       = [A-Z0-9_];
hibyte      = (0x80..0xff);

wordstart   = ascii_alpha | hibyte;
wordcont    = ascii_alnum | hibyte;

action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('S', tok, state->ctx); if (o!=OK) fbreak; }
}
action on_key {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('F', tok, state->ctx); if (o!=OK) fbreak; }
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('L', tok, state->ctx); if (o!=OK) fbreak; }
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('P', tok, state->ctx); if (o!=OK) fbreak; }
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('W', tok, state->ctx); if (o!=OK) fbreak; }
}

main := |*

    # ---- issue keys (ABC-123) — wins over plain word by longest match ----
    uc ucnum* "-" dgt+                                   => on_key;

    # ---- numbers ----
    "0" [xX] xdgt+                                       => on_number;
    dgt+ "." dgt*                                        => on_number;
    "." dgt+                                             => on_number;
    dgt+                                                 => on_number;

    # ---- words (ASCII + hi-byte runs, no strict UTF-8) ----
    wordstart wordcont*                                  => on_word;

    # ---- horizontal whitespace runs ----
    ws_h+                                                => on_space;

    # ---- newline: one byte, never fused with anything ----
    [\n]                                                 => on_space;

    # ---- catch-all: any other single byte → punctuation ----
    any8                                                 => on_punct;

*|;

}%%

%%write data;

ok64 FREELexer(FREEstate* state) {

    a_dup(u8c, data, state->data);
    sane($ok(data));

    int cs = 0;
    int act = 0;
    u8c *p = (u8c*) data[0];
    u8c *pe = (u8c*) data[1];
    u8c *eof = pe;
    u8c *ts = NULL;
    u8c *te = NULL;
    ok64 o = OK;

    u8cs tok = {p, p};

    %% write init;
    %% write exec;

    state->data[0] = p;
    if (o==OK && cs < FREE_first_final)
        o = FREEBAD;

    return o;
}
