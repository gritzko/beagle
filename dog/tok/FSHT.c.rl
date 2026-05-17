#include "abc/INT.h"
#include "abc/PRO.h"
#include "FSHT.h"

ok64 FSHTonComment (u8cs tok, FSHTstate* state);
ok64 FSHTonString (u8cs tok, FSHTstate* state);
ok64 FSHTonNumber (u8cs tok, FSHTstate* state);
ok64 FSHTonWord (u8cs tok, FSHTstate* state);
ok64 FSHTonPunct (u8cs tok, FSHTstate* state);
ok64 FSHTonSpace (u8cs tok, FSHTstate* state);

%%{

machine FSHT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9'];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abefnrtv\\'\"0]
           | [x] xdgt{2}
           | [u] xdgt{4}
           | [U] xdgt{8} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FSHTonSpace(tok, state);
    if (o!=OK) fbreak;
}

ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
bdig = [01] ( [_]? [01] )*;
odig = odgt ( [_]? odgt )*;

main := |*

    # ---- comments ----
    "//" [^\n]*                                                   => on_comment;
    "(*" ( any8 - [*] | [*]+ (any8 - [*)] ) )* [*]+ ")"         => on_comment;

    # ---- triple-quoted strings ----
    "\"\"\"" ( any8 - ["] | ["] (any8 - ["]) | ["]["] (any8 - ["]) )* "\"\"\""  => on_string;

    # ---- verbatim strings ----
    "@" ["] ( ["]["] | any8 - ["] )* ["]                          => on_string;

    # ---- regular strings ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- character literals ----
    ['] ( esc | any8 - ['\\] ) [']                                => on_string;

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
    "->" | "<-" | "|>" | "<|" | ">>" | "<<" |
    "::" | ":>" | ":?>" |
    "+=" | "-=" | "*=" | "/=" |
    "&&" | "||" | "<=" | ">=" | "==" | "!=" | "<>" |
    "..." | ".."                                                  => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'@] - [.])                    => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 FSHTLexer(FSHTstate* state) {

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
    if (o==OK && cs < FSHT_first_final)
        o = FSHTBAD;

    return o;
}
