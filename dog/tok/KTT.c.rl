#include "abc/INT.h"
#include "abc/PRO.h"
#include "KTT.h"

ok64 KTTonComment (u8cs tok, KTTstate* state);
ok64 KTTonString (u8cs tok, KTTstate* state);
ok64 KTTonNumber (u8cs tok, KTTstate* state);
ok64 KTTonAnnotation (u8cs tok, KTTstate* state);
ok64 KTTonWord (u8cs tok, KTTstate* state);
ok64 KTTonPunct (u8cs tok, KTTstate* state);
ok64 KTTonSpace (u8cs tok, KTTstate* state);

%%{

machine KTT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abefnrtv\\'\"?0$]
           | [x] xdgt{2}
           | [u] xdgt{4} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_annotation {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonAnnotation(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = KTTonSpace(tok, state);
    if (o!=OK) fbreak;
}

# digit groups with underscore separator
ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
bdig = [01] ( [_]? [01] )*;

# integer/float suffixes
isuf = [lLuU]?;
fsuf = [fFdD]?;

main := |*

    # ---- comments ----
    "//" [^\n]*                                                   => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- triple-quoted raw strings ----
    ["] ["] ["] ( any8 - ["] | ["] (any8 - ["]) | ["] ["] (any8 - ["]) )* ["] ["] ["]   => on_string;

    # ---- string literals ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- character literals ----
    ['] ( esc | any8 - ['\\] )* [']                               => on_string;

    # ---- numbers ----
    "0" [xX] xdig isuf                                            => on_number;
    "0" [bB] bdig isuf                                            => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)? fsuf                      => on_number;
    "." ddig ([eE] [+\-]? ddig)? fsuf                             => on_number;
    ddig [eE] [+\-]? ddig fsuf                                    => on_number;
    ddig isuf                                                      => on_number;

    # ---- annotations ----
    "@" idalpha ( idalnum | [.] )*                                => on_annotation;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    ">>=" | "<<=" |
    "+=" | "-=" | "*=" | "/=" | "%=" |
    "&=" | "|=" | "^=" |
    ">>" | "<<" | "++" | "--" |
    "->" | "::" | ".." |
    "&&" | "||" | "<=" | ">=" | "==" | "!=" |
    "?:" | "?." | "?:" | "=>"                                     => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'@] - [.])                    => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 KTTLexer(KTTstate* state) {

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
    if (o==OK && cs < KTT_first_final)
        o = KTTBAD;

    return o;
}
