#include "abc/INT.h"
#include "abc/PRO.h"
#include "TST.h"

ok64 TSTonComment (u8cs tok, TSTstate* state);
ok64 TSTonString (u8cs tok, TSTstate* state);
ok64 TSTonNumber (u8cs tok, TSTstate* state);
ok64 TSTonWord (u8cs tok, TSTstate* state);
ok64 TSTonPunct (u8cs tok, TSTstate* state);
ok64 TSTonSpace (u8cs tok, TSTstate* state);

%%{

machine TST;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_$];
idalnum = [a-zA-Z_$0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abefnrtv\\'\"?0/]
           | odgt{1,3}
           | [x] xdgt{2}
           | [u] xdgt{4}
           | [u] "{" xdgt{1,6} "}" );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TSTonSpace(tok, state);
    if (o!=OK) fbreak;
}

# digit groups with underscore separator
ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
bdig = [01] ( [_]? [01] )*;
odig = odgt ( [_]? odgt )*;

# BigInt suffix
nsuf = [n]?;

main := |*

    # ---- comments ----
    "//" [^\n]*                                                   => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- template strings (backtick) ----
    0x60 ( [\\] any8 | any8 - 0x60 - [\\] )* 0x60                => on_string;

    # ---- string literals ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;
    ['] ( esc | any8 - ['\\] )* [']                               => on_string;

    # ---- numbers ----
    "0" [xX] xdig nsuf                                            => on_number;
    "0" [oO] odig nsuf                                            => on_number;
    "0" [bB] bdig nsuf                                            => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)? nsuf                      => on_number;
    "." ddig ([eE] [+\-]? ddig)? nsuf                             => on_number;
    ddig [eE] [+\-]? ddig nsuf                                    => on_number;
    ddig nsuf                                                      => on_number;

    # ---- identifiers (including #private) ----
    [#]? idalpha idalnum*                                         => on_word;

    # ---- multi-char operators ----
    ">>>" | ">>>=" |
    ">>=" | "<<=" | "**=" |
    "+=" | "-=" | "*=" | "/=" | "%=" |
    "&=" | "|=" | "^=" |
    "&&=" | "||=" | "??=" |
    ">>" | "<<" | "**" |
    "++" | "--" | "=>" | "?." | "??" |
    "&&" | "||" | "<=" | ">=" | "==" | "!=" |
    "===" | "!==" | "..."                                         => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'#] - [.] - 0x60)             => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 TSTLexer(TSTstate* state) {

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
    if (o==OK && cs < TST_first_final)
        o = TSTBAD;

    return o;
}
