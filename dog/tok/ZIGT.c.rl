#include "abc/INT.h"
#include "abc/PRO.h"
#include "ZIGT.h"

ok64 ZIGTonComment (u8cs tok, ZIGTstate* state);
ok64 ZIGTonString (u8cs tok, ZIGTstate* state);
ok64 ZIGTonNumber (u8cs tok, ZIGTstate* state);
ok64 ZIGTonWord (u8cs tok, ZIGTstate* state);
ok64 ZIGTonPunct (u8cs tok, ZIGTstate* state);
ok64 ZIGTonSpace (u8cs tok, ZIGTstate* state);

%%{

machine ZIGT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abefnrtv\\'\"?0\\]
           | [x] xdgt{2}
           | [u] "{" xdgt{1,6} "}" );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ZIGTonSpace(tok, state);
    if (o!=OK) fbreak;
}

# digit groups with underscore separator
ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
odig = odgt ( [_]? odgt )*;
bdig = [01] ( [_]? [01] )*;

main := |*

    # ---- comments (line only, no block comments in Zig) ----
    "//" [^\n]*                                                   => on_comment;

    # ---- multiline strings (\\...newline continuation lines) ----
    [\\] [\\] [^\n]* ( [\n] [\\] [\\] [^\n]* )*                  => on_string;

    # ---- regular string literals ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- character literals ----
    ['] ( esc | any8 - ['\\] ) [']                                => on_string;

    # ---- numbers ----
    "0" [xX] xdig ( "." xdig )? ( [pP] [+\-]? ddig )?           => on_number;
    "0" [oO] odig                                                 => on_number;
    "0" [bB] bdig                                                 => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)?                            => on_number;
    "." ddig ([eE] [+\-]? ddig)?                                  => on_number;
    ddig [eE] [+\-]? ddig                                         => on_number;
    ddig                                                           => on_number;

    # ---- built-in functions @name ----
    "@" idalpha idalnum*                                          => on_word;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    ">>=" | "<<=" |
    "+=" | "-=" | "*=" | "/=" | "%=" |
    "&=" | "|=" | "^=" |
    "++" | ">>" | "<<" |
    "=>" | ".." | "..." |
    "&&" | "||" | "<=" | ">=" | "==" | "!=" |
    ".*" | ".?"                                                   => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'@] - [.])                    => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 ZIGTLexer(ZIGTstate* state) {

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
    if (o==OK && cs < ZIGT_first_final)
        o = ZIGTBAD;

    return o;
}
