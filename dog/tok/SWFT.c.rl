#include "abc/INT.h"
#include "abc/PRO.h"
#include "SWFT.h"

ok64 SWFTonComment (u8cs tok, SWFTstate* state);
ok64 SWFTonString (u8cs tok, SWFTstate* state);
ok64 SWFTonNumber (u8cs tok, SWFTstate* state);
ok64 SWFTonWord (u8cs tok, SWFTstate* state);
ok64 SWFTonPunct (u8cs tok, SWFTstate* state);
ok64 SWFTonSpace (u8cs tok, SWFTstate* state);

%%{

machine SWFT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abefnrtv\\'\"?0]
           | [x] xdgt{2}
           | [u] "{" xdgt{1,8} "}"
           | [\n] );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SWFTonSpace(tok, state);
    if (o!=OK) fbreak;
}

# digit groups with underscore separator
ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
bdig = [01] ( [_]? [01] )*;
odig = odgt ( [_]? odgt )*;

main := |*

    # ---- comments ----
    "//" [^\n]*                                                   => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- extended string delimiters #"..."# ----
    [#] ["] ( any8 - ["] | ["] (any8 - [#]) )* ["] [#]           => on_string;

    # ---- triple-quoted strings ----
    ["] ["] ["] ( any8 - ["] | ["] (any8 - ["]) | ["] ["] (any8 - ["]) )* ["] ["] ["]   => on_string;

    # ---- regular string literals ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- numbers ----
    "0" [xX] xdig                                                 => on_number;
    "0" [oO] odig                                                 => on_number;
    "0" [bB] bdig                                                 => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)?                            => on_number;
    "." ddig ([eE] [+\-]? ddig)?                                  => on_number;
    ddig [eE] [+\-]? ddig                                         => on_number;
    ddig                                                           => on_number;

    # ---- identifiers (including backtick-quoted) ----
    0x60 idalpha idalnum* 0x60                                    => on_word;
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    ">>=" | "<<=" |
    "+=" | "-=" | "*=" | "/=" | "%=" |
    "&=" | "|=" | "^=" |
    ">>" | "<<" |
    "->" | ".." | "..." |
    "&&" | "||" | "<=" | ">=" | "==" | "!=" |
    "??" | "?."                                                   => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["] - [.] - 0x60)               => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 SWFTLexer(SWFTstate* state) {

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
    if (o==OK && cs < SWFT_first_final)
        o = SWFTBAD;

    return o;
}
