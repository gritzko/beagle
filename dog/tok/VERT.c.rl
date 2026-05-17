#include "abc/INT.h"
#include "abc/PRO.h"
#include "VERT.h"

ok64 VERTonComment (u8cs tok, VERTstate* state);
ok64 VERTonString (u8cs tok, VERTstate* state);
ok64 VERTonNumber (u8cs tok, VERTstate* state);
ok64 VERTonPreproc (u8cs tok, VERTstate* state);
ok64 VERTonWord (u8cs tok, VERTstate* state);
ok64 VERTonPunct (u8cs tok, VERTstate* state);
ok64 VERTonSpace (u8cs tok, VERTstate* state);

%%{

machine VERT;

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
           | [x] xdgt+ );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_preproc {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonPreproc(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VERTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- comments ----
    "//" [^\n]*                                                   => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- strings ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- Verilog-style numbers: size'base_value ----
    dgt+ ['] [sS]? [bBoOdDhH] [0-9a-fA-F_xXzZ?]+               => on_number;
    ['] [sS]? [bBoOdDhH] [0-9a-fA-F_xXzZ?]+                    => on_number;
    dgt+ ("." dgt+)? ([eE] [+\-]? dgt+)?                         => on_number;

    # ---- preprocessor directives ----
    0x60 idalpha idalnum*                                         => on_preproc;

    # ---- system tasks $name ----
    "$" idalpha idalnum*                                          => on_word;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    "===" | "!==" | "==?" | "!=?" |
    ">>>" | "<<<" |
    ">>=" | "<<=" |
    "+=" | "-=" | "*=" | "/=" | "%=" |
    "&=" | "|=" | "^=" |
    ">>" | "<<" | "++" | "--" |
    "&&" | "||" | "<=" | ">=" | "==" | "!=" |
    "->" | "<->" | "=>" | "::" | "##" |
    "+:" | "-:" | "*>" | "(*" | "*)"                              => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["$] - [.] - 0x60)             => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 VERTLexer(VERTstate* state) {

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
    if (o==OK && cs < VERT_first_final)
        o = VERTBAD;

    return o;
}
