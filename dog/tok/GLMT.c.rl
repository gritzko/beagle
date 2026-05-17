#include "abc/INT.h"
#include "abc/PRO.h"
#include "GLMT.h"

ok64 GLMTonComment (u8cs tok, GLMTstate* state);
ok64 GLMTonString (u8cs tok, GLMTstate* state);
ok64 GLMTonNumber (u8cs tok, GLMTstate* state);
ok64 GLMTonWord (u8cs tok, GLMTstate* state);
ok64 GLMTonPunct (u8cs tok, GLMTstate* state);
ok64 GLMTonSpace (u8cs tok, GLMTstate* state);

%%{

machine GLMT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abefnrtv\\'\"0]
           | [x] xdgt{2}
           | [u] xdgt{4} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLMTonSpace(tok, state);
    if (o!=OK) fbreak;
}

ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
bdig = [01] ( [_]? [01] )*;
odig = odgt ( [_]? odgt )*;

main := |*

    # ---- comments ----
    "//" [^\n]*                                                   => on_comment;

    # ---- string literals ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- numbers ----
    "0" [xX] xdig                                                 => on_number;
    "0" [oO] odig                                                 => on_number;
    "0" [bB] bdig                                                 => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)?                            => on_number;
    "." ddig ([eE] [+\-]? ddig)?                                  => on_number;
    ddig [eE] [+\-]? ddig                                         => on_number;
    ddig                                                           => on_number;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    "->" | "<-" | "|>" | ".." | "<=" | ">=" | "==" | "!=" |
    "&&" | "||"                                                   => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["] - [.])                       => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 GLMTLexer(GLMTstate* state) {

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
    if (o==OK && cs < GLMT_first_final)
        o = GLMTBAD;

    return o;
}
