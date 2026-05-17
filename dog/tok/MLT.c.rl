#include "abc/INT.h"
#include "abc/PRO.h"
#include "MLT.h"

ok64 MLTonComment (u8cs tok, MLTstate* state);
ok64 MLTonString (u8cs tok, MLTstate* state);
ok64 MLTonNumber (u8cs tok, MLTstate* state);
ok64 MLTonWord (u8cs tok, MLTstate* state);
ok64 MLTonPunct (u8cs tok, MLTstate* state);
ok64 MLTonSpace (u8cs tok, MLTstate* state);

%%{

machine MLT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9'];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abefnrtv\\'\"\\]
           | [x] xdgt{2}
           | dgt{3}
           | [\n] [ \t]* );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MLTonSpace(tok, state);
    if (o!=OK) fbreak;
}

ddig = dgt ( [_]? dgt )*;

main := |*

    # ---- block comments (* ... *) -- not nested for simplicity ----
    "(*" ( any8 - [*] | [*] (any8 - [)]) )* "*)"                => on_comment;

    # ---- strings ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- char literals ----
    ['] ( esc | any8 - ['\\] ) [']                                => on_string;

    # ---- numbers ----
    "0" [xX] xdgt ( [_]? xdgt )*                                 => on_number;
    "0" [oO] odgt ( [_]? odgt )*                                  => on_number;
    "0" [bB] [01] ( [_]? [01] )*                                  => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)?                            => on_number;
    ddig [eE] [+\-]? ddig                                         => on_number;
    ddig                                                           => on_number;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- operator symbols ----
    [!$%&*+\-.\/<=>?@^|~:]+                                      => on_punct;

    # ---- punctuation ----
    [(){}[\],;#]                                                  => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 MLTLexer(MLTstate* state) {

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
    if (o==OK && cs < MLT_first_final)
        o = MLTBAD;

    return o;
}
