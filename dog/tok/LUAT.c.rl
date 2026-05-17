#include "abc/INT.h"
#include "abc/PRO.h"
#include "LUAT.h"

ok64 LUATonComment (u8cs tok, LUATstate* state);
ok64 LUATonString (u8cs tok, LUATstate* state);
ok64 LUATonNumber (u8cs tok, LUATstate* state);
ok64 LUATonWord (u8cs tok, LUATstate* state);
ok64 LUATonPunct (u8cs tok, LUATstate* state);
ok64 LUATonSpace (u8cs tok, LUATstate* state);

%%{

machine LUAT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];

esc = [\\] ( [abfnrtv\\"'\n0]
           | [x] xdgt{2}
           | [z] ws*
           | dgt{1,3}
           | [u] "{" xdgt+ "}" );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LUATonSpace(tok, state);
    if (o!=OK) fbreak;
}

ddig = dgt+;
xdig = xdgt+;

main := |*

    # ---- block comment --[[ ... ]] ----
    "--[[" ( any8 - [\]] | [\]] (any8 - [\]]) )* "]]"            => on_comment;
    # ---- line comment ----
    "--" [^\n]*                                                    => on_comment;

    # ---- long strings [[ ... ]] ----
    "[[" ( any8 - [\]] | [\]] (any8 - [\]]) )* "]]"              => on_string;

    # ---- regular strings ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;
    ['] ( esc | any8 - ['\\] )* [']                               => on_string;

    # ---- numbers ----
    "0" [xX] xdig ("." xdig?)? ([pP] [+\-]? ddig)?               => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)?                            => on_number;
    "." ddig ([eE] [+\-]? ddig)?                                  => on_number;
    ddig [eE] [+\-]? ddig                                         => on_number;
    "0" [xX] xdig                                                 => on_number;
    ddig                                                           => on_number;

    # ---- identifiers ----
    idalpha idalnum*                                               => on_word;

    # ---- multi-char operators ----
    ".." | "..." | "==" | "~=" | "<=" | ">=" | "<<" | ">>" |
    "//" | "::" | "->"                                             => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'] - [.])                      => on_punct;
    [.]                                                            => on_punct;

    # ---- whitespace ----
    ws+                                                            => on_space;

*|;

}%%

%%write data;

ok64 LUATLexer(LUATstate* state) {

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
    if (o==OK && cs < LUAT_first_final)
        o = LUATBAD;

    return o;
}
