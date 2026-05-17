#include "abc/INT.h"
#include "abc/PRO.h"
#include "ERLT.h"

ok64 ERLTonComment (u8cs tok, ERLTstate* state);
ok64 ERLTonString (u8cs tok, ERLTstate* state);
ok64 ERLTonNumber (u8cs tok, ERLTstate* state);
ok64 ERLTonPreproc (u8cs tok, ERLTstate* state);
ok64 ERLTonWord (u8cs tok, ERLTstate* state);
ok64 ERLTonPunct (u8cs tok, ERLTstate* state);
ok64 ERLTonSpace (u8cs tok, ERLTstate* state);

%%{

machine ERLT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];

esc = [\\] ( [abdefnrstv\\'\"?0\n]
           | [x] xdgt{2}
           | dgt{1,3} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_preproc {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonPreproc(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = ERLTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- comments (% to end of line) ----
    [%] [^\n]*                                                     => on_comment;

    # ---- strings ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- quoted atoms ----
    ['] ( esc | any8 - ['\\] )* [']                               => on_string;

    # ---- character literal ($c) ----
    "$" [\\] any8                                                  => on_number;
    "$" (any8 - [\\])                                              => on_number;

    # ---- numbers ----
    # base#digits (e.g. 16#FF, 2#1010)
    dgt+ [#] [0-9a-zA-Z]+                                         => on_number;
    # float
    dgt+ "." dgt+ ([eE] [+\-]? dgt+)?                             => on_number;
    # plain integer
    dgt+                                                           => on_number;

    # ---- preprocessor directives ----
    "-" ("module"|"export"|"export_type"|"import"|"define"|"include"|"include_lib"|"ifdef"|"ifndef"|"else"|"endif"|"undef"|"record"|"type"|"opaque"|"spec"|"callback"|"behaviour"|"behavior") => on_preproc;

    # ---- atoms (lowercase start) ----
    [a-z] idalnum*                                                 => on_word;

    # ---- variables (uppercase start) ----
    [A-Z_] idalnum*                                                => on_word;

    # ---- multi-char operators ----
    "=>" | ":=" | "->" | "<-" | "=:=" | "=/=" |
    "==" | "/=" | ">=" | "=<" |
    "++" | "--" | "||" | "<<" | ">>" |
    "::" | ".." | "!"                                              => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'%$] - [.])                   => on_punct;
    [.]                                                            => on_punct;

    # ---- whitespace ----
    ws+                                                            => on_space;

*|;

}%%

%%write data;

ok64 ERLTLexer(ERLTstate* state) {

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
    if (o==OK && cs < ERLT_first_final)
        o = ERLTBAD;

    return o;
}
