#include "abc/INT.h"
#include "abc/PRO.h"
#include "PHPT.h"

ok64 PHPTonComment (u8cs tok, PHPTstate* state);
ok64 PHPTonString (u8cs tok, PHPTstate* state);
ok64 PHPTonNumber (u8cs tok, PHPTstate* state);
ok64 PHPTonPreproc (u8cs tok, PHPTstate* state);
ok64 PHPTonWord (u8cs tok, PHPTstate* state);
ok64 PHPTonPunct (u8cs tok, PHPTstate* state);
ok64 PHPTonSpace (u8cs tok, PHPTstate* state);

%%{

machine PHPT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abefnrtv\\'\"$]
           | [x] xdgt{1,2}
           | [u] "{" xdgt{1,6} "}"
           | odgt{1,3} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_preproc {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonPreproc(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PHPTonSpace(tok, state);
    if (o!=OK) fbreak;
}

ddig = dgt ( [_]? dgt )*;

main := |*

    # ---- PHP open/close tags ----
    "<?php" | "<?=" | "<?" | "?>"                                => on_preproc;

    # ---- comments ----
    "//" [^\n]*                                                   => on_comment;
    [#] [^\n]*                                                    => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- strings ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;
    ['] ( [\\] [\\'] | any8 - ['\\] )* [']                       => on_string;

    # ---- numbers ----
    "0" [xX] xdgt ( [_]? xdgt )*                                 => on_number;
    "0" [oO] odgt ( [_]? odgt )*                                  => on_number;
    "0" [bB] [01] ( [_]? [01] )*                                  => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)?                            => on_number;
    "." ddig ([eE] [+\-]? ddig)?                                   => on_number;
    ddig [eE] [+\-]? ddig                                         => on_number;
    ddig                                                           => on_number;

    # ---- variables ----
    "$" idalpha idalnum*                                          => on_word;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    "===" | "!==" | "<=>" | "??" | "??=" | "?->" |
    ">>=" | "<<=" | "**=" |
    "+=" | "-=" | "*=" | "/=" | "%=" | ".=" |
    "&=" | "|=" | "^=" |
    ">>" | "<<" | "++" | "--" | "**" |
    "->" | "::" | "=>" |
    "&&" | "||" | "<=" | ">=" | "==" | "!=" |
    "..."                                                         => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'#$] - [.])                   => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 PHPTLexer(PHPTstate* state) {

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
    if (o==OK && cs < PHPT_first_final)
        o = PHPTBAD;

    return o;
}
