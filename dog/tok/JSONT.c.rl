#include "abc/INT.h"
#include "abc/PRO.h"
#include "JSONT.h"

ok64 JSONTonString (u8cs tok, JSONTstate* state);
ok64 JSONTonNumber (u8cs tok, JSONTstate* state);
ok64 JSONTonKeyword (u8cs tok, JSONTstate* state);
ok64 JSONTonPunct (u8cs tok, JSONTstate* state);
ok64 JSONTonSpace (u8cs tok, JSONTstate* state);

%%{

machine JSONT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
dgt = [0-9];
xdgt = [0-9a-fA-F];

esc = [\\] ( ["\\bfnrt/]
           | [u] xdgt{4} );

action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSONTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSONTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_keyword {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSONTonKeyword(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSONTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSONTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- strings ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- numbers ----
    [\-]? dgt+ ("." dgt+)? ([eE] [+\-]? dgt+)?                   => on_number;

    # ---- keywords ----
    "true" | "false" | "null"                                     => on_keyword;

    # ---- punctuation ----
    [{}[\]:,]                                                     => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 JSONTLexer(JSONTstate* state) {

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
    if (o==OK && cs < JSONT_first_final)
        o = JSONTBAD;

    return o;
}
