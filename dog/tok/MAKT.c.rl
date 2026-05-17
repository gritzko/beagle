#include "abc/INT.h"
#include "abc/PRO.h"
#include "MAKT.h"

ok64 MAKTonComment (u8cs tok, MAKTstate* state);
ok64 MAKTonString (u8cs tok, MAKTstate* state);
ok64 MAKTonNumber (u8cs tok, MAKTstate* state);
ok64 MAKTonVar (u8cs tok, MAKTstate* state);
ok64 MAKTonWord (u8cs tok, MAKTstate* state);
ok64 MAKTonPunct (u8cs tok, MAKTstate* state);
ok64 MAKTonSpace (u8cs tok, MAKTstate* state);

%%{

machine MAKT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_var {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonVar(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MAKTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- comments ----
    [#] [^\n]*                                                    => on_comment;

    # ---- string literals ----
    ["] ( [\\] any8 | any8 - ["\\] )* ["]                        => on_string;
    ['] ( any8 - ['] )* [']                                       => on_string;

    # ---- variable references ----
    "$(" ( any8 - [)] )* ")"                                      => on_var;
    "${" ( any8 - [}] )* "}"                                      => on_var;
    "$" [@<\^?*%]                                                 => on_var;

    # ---- numbers ----
    dgt+                                                           => on_number;

    # ---- keywords with dot prefix ----
    "." idalpha idalnum*                                          => on_word;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;
    "-" idalpha idalnum*                                          => on_word;

    # ---- multi-char operators ----
    ":=" | "?=" | "+=" | "::=" | "!="                             => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'#$] - [.\-])                 => on_punct;
    [.\-]                                                         => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 MAKTLexer(MAKTstate* state) {

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
    if (o==OK && cs < MAKT_first_final)
        o = MAKTBAD;

    return o;
}
