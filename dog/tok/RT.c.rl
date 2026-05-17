#include "abc/INT.h"
#include "abc/PRO.h"
#include "RT.h"

ok64 RTonComment (u8cs tok, RTstate* state);
ok64 RTonString (u8cs tok, RTstate* state);
ok64 RTonNumber (u8cs tok, RTstate* state);
ok64 RTonWord (u8cs tok, RTstate* state);
ok64 RTonPunct (u8cs tok, RTstate* state);
ok64 RTonSpace (u8cs tok, RTstate* state);

%%{

machine RT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_.];
idalnum = [a-zA-Z_.0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];

esc = [\\] ( [abfnrtv\\'\"?0\n]
           | [x] xdgt{2}
           | [u] xdgt{4}
           | [U] xdgt{8} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RTonSpace(tok, state);
    if (o!=OK) fbreak;
}

ddig = dgt+;
xdig = xdgt+;

main := |*

    # ---- comments ----
    [#] [^\n]*                                                     => on_comment;

    # ---- raw strings ----
    [rR] ["] ( any8 - ["] )* ["]                                  => on_string;
    [rR] ['] ( any8 - ['] )* [']                                  => on_string;

    # ---- regular strings ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;
    ['] ( esc | any8 - ['\\] )* [']                               => on_string;

    # ---- numbers ----
    "0" [xX] xdig [iL]?                                           => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)? [iL]?                     => on_number;
    "." ddig ([eE] [+\-]? ddig)? [iL]?                            => on_number;
    ddig [eE] [+\-]? ddig [iL]?                                   => on_number;
    ddig [iL]?                                                     => on_number;

    # ---- identifiers (R allows . in identifiers) ----
    [a-zA-Z] idalnum*                                              => on_word;
    [.] [a-zA-Z_] idalnum*                                        => on_word;

    # ---- multi-char operators ----
    "<-" | "<<-" | "->" | "->>" |
    "::" | ":::" | "~" |
    "&&" | "||" | "<=" | ">=" | "==" | "!=" |
    "%%" | "%*%" | "%/%" | "%in%" | "%o%" | "%x%" |
    "..." | "|>"                                                   => on_punct;

    # ---- single-char punctuation ----
    (any8 - [a-zA-Z_] - dgt - ws - ["'#] - [.])                  => on_punct;
    [.]                                                            => on_punct;

    # ---- whitespace ----
    ws+                                                            => on_space;

*|;

}%%

%%write data;

ok64 RTLexer(RTstate* state) {

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
    if (o==OK && cs < RT_first_final)
        o = RTBAD;

    return o;
}
