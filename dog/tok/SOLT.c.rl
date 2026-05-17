#include "abc/INT.h"
#include "abc/PRO.h"
#include "SOLT.h"

ok64 SOLTonComment (u8cs tok, SOLTstate* state);
ok64 SOLTonString (u8cs tok, SOLTstate* state);
ok64 SOLTonNumber (u8cs tok, SOLTstate* state);
ok64 SOLTonWord (u8cs tok, SOLTstate* state);
ok64 SOLTonPunct (u8cs tok, SOLTstate* state);
ok64 SOLTonSpace (u8cs tok, SOLTstate* state);

%%{

machine SOLT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_$];
idalnum = [a-zA-Z_$0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];

esc = [\\] ( [abefnrtv\\'\"0]
           | [x] xdgt{2}
           | [u] xdgt{4} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SOLTonSpace(tok, state);
    if (o!=OK) fbreak;
}

ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;

main := |*

    # ---- comments ----
    "//" [^\n]*                                                   => on_comment;
    "///" [^\n]*                                                  => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- string literals ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;
    ['] ( esc | any8 - ['\\] )* [']                               => on_string;

    # ---- numbers ----
    "0" [xX] xdig                                                 => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)?                            => on_number;
    "." ddig ([eE] [+\-]? ddig)?                                  => on_number;
    ddig [eE] [+\-]? ddig                                         => on_number;
    ddig                                                           => on_number;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    ">>=" | "<<=" | "+=" | "-=" | "*=" | "/=" | "%=" |
    "&=" | "^=" | "|=" | ">>" | "<<" | "++" | "--" |
    "=>" | "&&" | "||" | "<=" | ">=" | "==" | "!=" |
    "**"                                                          => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'] - [.])                      => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 SOLTLexer(SOLTstate* state) {

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
    if (o==OK && cs < SOLT_first_final)
        o = SOLTBAD;

    return o;
}
