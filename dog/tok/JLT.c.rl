#include "abc/INT.h"
#include "abc/PRO.h"
#include "JLT.h"

ok64 JLTonComment (u8cs tok, JLTstate* state);
ok64 JLTonString (u8cs tok, JLTstate* state);
ok64 JLTonNumber (u8cs tok, JLTstate* state);
ok64 JLTonWord (u8cs tok, JLTstate* state);
ok64 JLTonPunct (u8cs tok, JLTstate* state);
ok64 JLTonSpace (u8cs tok, JLTstate* state);

%%{

machine JLT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9!];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abefnrtv\\'\"0$]
           | [x] xdgt{2}
           | [u] xdgt{4}
           | [U] xdgt{8}
           | [\n] );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JLTonSpace(tok, state);
    if (o!=OK) fbreak;
}

ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
odig = odgt ( [_]? odgt )*;
bdig = [01] ( [_]? [01] )*;

main := |*

    # ---- nested block comments #= ... =# (flat approximation) ----
    "#=" ( any8 - [=] | [=] (any8 - [#]) )* "=#"                => on_comment;

    # ---- line comments ----
    [#] [^\n]*                                                    => on_comment;

    # ---- triple-quoted strings ----
    ["] ["] ["] ( any8 - ["] | ["] (any8 - ["]) | ["] ["] (any8 - ["]) )* ["] ["] ["]   => on_string;

    # ---- strings ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- char literals ----
    ['] ( esc | any8 - ['\\] ) [']                                => on_string;

    # ---- raw strings ----
    "raw" ["] ( any8 - ["] )* ["]                                 => on_string;

    # ---- numbers ----
    "0" [xX] xdig ("." xdig?)? ([pP] [+\-]? ddig)?              => on_number;
    "0" [oO] odig                                                 => on_number;
    "0" [bB] bdig                                                 => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)? ("im")?                   => on_number;
    "." ddig ([eE] [+\-]? ddig)? ("im")?                          => on_number;
    ddig [eE] [+\-]? ddig ("im")?                                 => on_number;
    ddig ("im")?                                                   => on_number;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    "::" | "<:" | ">:" | ".+" | ".-" | ".*" | "./" |
    ".^" | ".%" | ".<<" | ".>>" | ".==" | ".!=" |
    ".<=" | ".>=" | ".<" | ".>" |
    "+=" | "-=" | "*=" | "/=" | "%=" | "^=" |
    "&=" | "|=" | "<<=" | ">>=" |
    "&&" | "||" | "<=" | ">=" | "==" | "!=" |
    "===" | "!==" |
    ">>" | "<<" | "..." | ".." | "=>"                            => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'#] - [.])                    => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 JLTLexer(JLTstate* state) {

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
    if (o==OK && cs < JLT_first_final)
        o = JLTBAD;

    return o;
}
