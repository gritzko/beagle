#include "abc/INT.h"
#include "abc/PRO.h"
#include "GOT.h"

ok64 GOTonComment (u8cs tok, GOTstate* state);
ok64 GOTonString (u8cs tok, GOTstate* state);
ok64 GOTonNumber (u8cs tok, GOTstate* state);
ok64 GOTonWord (u8cs tok, GOTstate* state);
ok64 GOTonPunct (u8cs tok, GOTstate* state);
ok64 GOTonSpace (u8cs tok, GOTstate* state);

%%{

machine GOT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abefnrtv\\'\"0]
           | odgt{1,3}
           | [x] xdgt{2}
           | [u] xdgt{4}
           | [U] xdgt{8} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GOTonSpace(tok, state);
    if (o!=OK) fbreak;
}

# digit groups with underscore separator
ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
odig = odgt ( [_]? odgt )*;
bdig = [01] ( [_]? [01] )*;

# suffixes
isuf = [i]?;

main := |*

    # ---- comments ----
    "//" [^\n]*                                                   => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- string literals ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- raw string literals (backtick) ----
    0x60 ( any8 - 0x60 )* 0x60                                    => on_string;

    # ---- rune literals ----
    ['] ( esc | any8 - ['\\] )* [']                               => on_string;

    # ---- numbers ----
    "0" [xX] xdig ("." xdig?)? ([pP] [+\-]? ddig)? isuf         => on_number;
    "0" [oO] odig isuf                                            => on_number;
    "0" [bB] bdig isuf                                            => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)? isuf                      => on_number;
    "." ddig ([eE] [+\-]? ddig)? isuf                             => on_number;
    ddig [eE] [+\-]? ddig isuf                                    => on_number;
    ddig isuf                                                      => on_number;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    ":=" | "<-" | "<<" | ">>" | "&^" |
    "<<=" | ">>=" | "&^=" |
    "+=" | "-=" | "*=" | "/=" | "%=" |
    "&=" | "|=" | "^=" |
    "&&" | "||" | "<=" | ">=" | "==" | "!=" |
    "++" | "--" | "->" | "..." |
    "::"                                                          => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'] - 0x60 - [.])              => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 GOTLexer(GOTstate* state) {

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
    if (o==OK && cs < GOT_first_final)
        o = GOTBAD;

    return o;
}
