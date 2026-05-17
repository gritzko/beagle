#include "abc/INT.h"
#include "abc/PRO.h"
#include "JAT.h"

ok64 JATonComment (u8cs tok, JATstate* state);
ok64 JATonString (u8cs tok, JATstate* state);
ok64 JATonNumber (u8cs tok, JATstate* state);
ok64 JATonAnnotation (u8cs tok, JATstate* state);
ok64 JATonWord (u8cs tok, JATstate* state);
ok64 JATonPunct (u8cs tok, JATstate* state);
ok64 JATonSpace (u8cs tok, JATstate* state);

%%{

machine JAT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_$];
idalnum = [a-zA-Z_$0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abefnrtv\\'\"?0]
           | odgt{1,3}
           | [x] xdgt+
           | [u] xdgt{4} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_annotation {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonAnnotation(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JATonSpace(tok, state);
    if (o!=OK) fbreak;
}

# digit groups with underscore separator
ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
bdig = [01] ( [_]? [01] )*;

# integer suffixes: l/L
isuf = [lL]?;

# float suffixes: f F d D
fsuf = [fFdD]?;

main := |*

    # ---- comments ----
    "//" [^\n]*                                                   => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- text blocks (triple-quoted strings) ----
    ["] ["] ["] ( any8 - ["] | ["] (any8 - ["]) | ["] ["] (any8 - ["]) )* ["] ["] ["]   => on_string;

    # ---- string literals ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- character literals ----
    ['] ( esc | any8 - ['\\] )* [']                               => on_string;

    # ---- numbers ----
    "0" [xX] xdig ("." xdig?)? ([pP] [+\-]? ddig)? fsuf         => on_number;
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
    ">>>" | ">>>=" |
    ">>=" | "<<=" |
    "+=" | "-=" | "*=" | "/=" | "%=" |
    "&=" | "|=" | "^=" |
    ">>" | "<<" | "++" | "--" |
    "->" | "::" | "&&" | "||" |
    "<=" | ">=" | "==" | "!=" | "..."                             => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'@] - [.])                    => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 JATLexer(JATstate* state) {

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
    if (o==OK && cs < JAT_first_final)
        o = JATBAD;

    return o;
}
