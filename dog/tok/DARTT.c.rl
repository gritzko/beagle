#include "abc/INT.h"
#include "abc/PRO.h"
#include "DARTT.h"

ok64 DARTTonComment (u8cs tok, DARTTstate* state);
ok64 DARTTonString (u8cs tok, DARTTstate* state);
ok64 DARTTonNumber (u8cs tok, DARTTstate* state);
ok64 DARTTonAnnotation (u8cs tok, DARTTstate* state);
ok64 DARTTonWord (u8cs tok, DARTTstate* state);
ok64 DARTTonPunct (u8cs tok, DARTTstate* state);
ok64 DARTTonSpace (u8cs tok, DARTTstate* state);

%%{

machine DARTT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_$];
idalnum = [a-zA-Z_$0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abefnrtv\\'\"?0$]
           | [x] xdgt{2}
           | [u] "{" xdgt{1,6} "}" );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_annotation {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonAnnotation(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DARTTonSpace(tok, state);
    if (o!=OK) fbreak;
}

# digit groups with underscore separator (Dart uses _ in numbers too, but less common)
ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;

main := |*

    # ---- comments ----
    "//" [^\n]*                                                   => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- triple-quoted strings (double quote) ----
    ["] ["] ["] ( any8 - ["] | ["] (any8 - ["]) | ["] ["] (any8 - ["]) )* ["] ["] ["]   => on_string;

    # ---- triple-quoted strings (single quote) ----
    ['] ['] ['] ( any8 - ['] | ['] (any8 - [']) | ['] ['] (any8 - [']) )* ['] ['] [']   => on_string;

    # ---- raw strings r"..." and r'...' ----
    [r] ["] ( any8 - ["] )* ["]                                   => on_string;
    [r] ['] ( any8 - ['] )* [']                                   => on_string;

    # ---- regular string literals ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;
    ['] ( esc | any8 - ['\\] )* [']                               => on_string;

    # ---- numbers ----
    "0" [xX] xdig                                                 => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)?                            => on_number;
    "." ddig ([eE] [+\-]? ddig)?                                  => on_number;
    ddig [eE] [+\-]? ddig                                         => on_number;
    ddig                                                           => on_number;

    # ---- annotations ----
    "@" idalpha ( idalnum | [.] )*                                => on_annotation;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    ">>=" | "<<=" |
    "+=" | "-=" | "*=" | "/=" | "%=" | "~/=" |
    "&=" | "|=" | "^=" |
    ">>" | "<<" | "~/" |
    "++" | "--" | "=>" |
    "&&" | "||" | "<=" | ">=" | "==" | "!=" |
    "?." | "??" | "??=" | ".." | "..."                            => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'@] - [.])                    => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 DARTTLexer(DARTTstate* state) {

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
    if (o==OK && cs < DARTT_first_final)
        o = DARTTBAD;

    return o;
}
