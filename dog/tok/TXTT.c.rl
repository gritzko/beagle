#include "abc/INT.h"
#include "abc/PRO.h"
#include "TXTT.h"

%%{

machine TXTT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];

action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('S', tok, state->ctx); if (o!=OK) fbreak; }
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('P', tok, state->ctx); if (o!=OK) fbreak; }
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    if (state->cb) { o = state->cb('S', tok, state->ctx); if (o!=OK) fbreak; }
}

main := |*

    # ---- words ----
    idalpha idalnum*                => on_word;

    # ---- whitespace ----
    ws+                             => on_space;

    # ---- everything else is punctuation ----
    (any8 - idalpha - ws)           => on_punct;

*|;

}%%

%%write data;

ok64 TXTTLexer(TXTTstate* state) {

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
    if (o==OK && cs < TXTT_first_final)
        o = TXTTBAD;

    return o;
}
