#include "abc/INT.h"
#include "abc/PRO.h"
#include "ELXT.h"

ok64 ELXTonComment (u8cs tok, ELXTstate* state);
ok64 ELXTonString (u8cs tok, ELXTstate* state);
ok64 ELXTonNumber (u8cs tok, ELXTstate* state);
ok64 ELXTonDecorator (u8cs tok, ELXTstate* state);
ok64 ELXTonWord (u8cs tok, ELXTstate* state);
ok64 ELXTonPunct (u8cs tok, ELXTstate* state);
ok64 ELXTonSpace (u8cs tok, ELXTstate* state);

%%{

machine ELXT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];

esc = [\\] ( [abefnrstv\\'\"?0\n]
           | [x] xdgt{2}
           | [u] xdgt{4} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_decorator {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonDecorator(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ELXTonSpace(tok, state);
    if (o!=OK) fbreak;
}

ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
bdig = [01] ( [_]? [01] )*;
odig = [0-7] ( [_]? [0-7] )*;

main := |*

    # ---- comments ----
    [#] [^\n]*                                                     => on_comment;

    # ---- heredoc strings (triple-quoted) ----
    ["] ["] ["] ( any8 - ["] | ["] (any8 - ["]) | ["] ["] (any8 - ["]) )* ["] ["] ["]  => on_string;
    ['] ['] ['] ( any8 - ['] | ['] (any8 - [']) | ['] ['] (any8 - [']) )* ['] ['] [']  => on_string;

    # ---- sigils ----
    "~" [rRsSwWcC] [/] ( [\\] any8 | any8 - [/\\] )* [/] [a-zA-Z]*   => on_string;
    "~" [rRsSwWcC] ["] ( [\\] any8 | any8 - ["\\] )* ["] [a-zA-Z]*  => on_string;

    # ---- regular strings ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;
    ['] ( any8 - ['\\] | [\\] any8 )* [']                        => on_string;

    # ---- atoms ----
    ":" idalpha idalnum* [?!]?                                     => on_string;
    ":" ["] ( esc | any8 - ["\\] )* ["]                           => on_string;

    # ---- numbers ----
    "0" [xX] xdig                                                  => on_number;
    "0" [oO] odig                                                  => on_number;
    "0" [bB] bdig                                                  => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)?                             => on_number;
    ddig [eE] [+\-]? ddig                                          => on_number;
    ddig                                                            => on_number;

    # ---- module attributes (decorators) ----
    "@" idalpha idalnum*                                           => on_decorator;

    # ---- identifiers ----
    idalpha idalnum* [?!]?                                         => on_word;

    # ---- multi-char operators ----
    "=>" | "->" | "|>" | "<>" | "<-" |
    "++" | "--" | "**" | ".." |
    "&&" | "||" | "==" | "!=" | "<=" | ">=" |
    "===" | "!==" | "<<<" | ">>>" |
    "<<" | ">>" | "::" |
    "+=" | "-=" | "*=" | "/=" |
    "&&=" | "||=" | "<~" | "~>" | "<~>" |
    "~~~" | "&&&" | "|||" |
    "..."                                                          => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'#@:~] - [.])                 => on_punct;
    [.] | [:]                                                      => on_punct;

    # ---- whitespace ----
    ws+                                                            => on_space;

*|;

}%%

%%write data;

ok64 ELXTLexer(ELXTstate* state) {

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
    if (o==OK && cs < ELXT_first_final)
        o = ELXTBAD;

    return o;
}
