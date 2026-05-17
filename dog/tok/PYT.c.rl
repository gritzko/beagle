#include "abc/INT.h"
#include "abc/PRO.h"
#include "PYT.h"

ok64 PYTonComment (u8cs tok, PYTstate* state);
ok64 PYTonString (u8cs tok, PYTstate* state);
ok64 PYTonNumber (u8cs tok, PYTstate* state);
ok64 PYTonDecorator (u8cs tok, PYTstate* state);
ok64 PYTonWord (u8cs tok, PYTstate* state);
ok64 PYTonPunct (u8cs tok, PYTstate* state);
ok64 PYTonSpace (u8cs tok, PYTstate* state);

%%{

machine PYT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] any8;

# string prefixes
strpfx = [rRbBuUfF]{0,2};

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_decorator {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonDecorator(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PYTonSpace(tok, state);
    if (o!=OK) fbreak;
}

# digit groups with underscore separator
ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
odig = odgt ( [_]? odgt )*;
bdig = [01] ( [_]? [01] )*;

main := |*

    # ---- comments (# to end of line) ----
    [#] [^\n]*                                                    => on_comment;

    # ---- triple-quoted strings ----
    strpfx? ["] ["] ["] ( any8 - ["] | ["] (any8 - ["]) | ["] ["] (any8 - ["]) )* ["] ["] ["]   => on_string;
    strpfx? ['] ['] ['] ( any8 - ['] | ['] (any8 - [']) | ['] ['] (any8 - [']) )* ['] ['] [']   => on_string;

    # ---- regular strings ----
    strpfx? ["] ( esc | any8 - ["\\] )* ["]                      => on_string;
    strpfx? ['] ( esc | any8 - ['\\] )* [']                      => on_string;

    # ---- numbers ----
    "0" [xX] xdig                                                 => on_number;
    "0" [oO] odig                                                 => on_number;
    "0" [bB] bdig                                                 => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)? [jJ]?                     => on_number;
    "." ddig ([eE] [+\-]? ddig)? [jJ]?                            => on_number;
    ddig [eE] [+\-]? ddig [jJ]?                                   => on_number;
    ddig [jJ]?                                                     => on_number;

    # ---- decorators ----
    "@" idalpha ( idalnum | [.] )*                                => on_decorator;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    "**=" | "//=" | "<<=" | ">>=" |
    "+=" | "-=" | "*=" | "/=" | "%=" |
    "&=" | "|=" | "^=" | "@=" |
    "**" | "//" | "<<" | ">>" |
    ":=" | "==" | "!=" | "<=" | ">=" |
    "&&" | "||" | "->" | "..."                                    => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - [#] - [.])                      => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 PYTLexer(PYTstate* state) {

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
    if (o==OK && cs < PYT_first_final)
        o = PYTBAD;

    return o;
}
