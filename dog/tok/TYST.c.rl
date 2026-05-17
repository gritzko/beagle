#include "abc/INT.h"
#include "abc/PRO.h"
#include "TYST.h"

ok64 TYSTonComment (u8cs tok, TYSTstate* state);
ok64 TYSTonString (u8cs tok, TYSTstate* state);
ok64 TYSTonNumber (u8cs tok, TYSTstate* state);
ok64 TYSTonMath (u8cs tok, TYSTstate* state);
ok64 TYSTonLabel (u8cs tok, TYSTstate* state);
ok64 TYSTonPreproc (u8cs tok, TYSTstate* state);
ok64 TYSTonWord (u8cs tok, TYSTstate* state);
ok64 TYSTonPunct (u8cs tok, TYSTstate* state);
ok64 TYSTonSpace (u8cs tok, TYSTstate* state);

%%{

machine TYST;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9\-];
dgt = [0-9];
xdgt = [0-9a-fA-F];

esc = [\\] ( [abefnrtv\\'\"0]
           | [x] xdgt{2}
           | [u] xdgt{4} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_math {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonMath(tok, state);
    if (o!=OK) fbreak;
}
action on_label {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonLabel(tok, state);
    if (o!=OK) fbreak;
}
action on_preproc {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonPreproc(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TYSTonSpace(tok, state);
    if (o!=OK) fbreak;
}

unit = ("pt" | "mm" | "cm" | "in" | "em");

main := |*

    # ---- comments ----
    "//" [^\n]*                                                   => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- string literals ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- math mode ----
    "$" ( [\\] any8 | any8 - [$\\] )* "$"                        => on_math;

    # ---- labels ----
    "<" idalpha idalnum* ">"                                      => on_label;

    # ---- code mode directives ----
    [#] ("if" | "else" | "for" | "while" | "let" | "set" |
         "show" | "import" | "include")                           => on_preproc;

    # ---- numbers with optional units ----
    dgt+ "." dgt+ unit?                                           => on_number;
    "." dgt+ unit?                                                => on_number;
    dgt+ unit?                                                    => on_number;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    "=>" | "+=" | "-=" | "*=" | "/=" |
    "==" | "!=" | "<=" | ">=" | ".."                              => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["<#] - [.])                    => on_punct;
    [.#]                                                          => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 TYSTLexer(TYSTstate* state) {

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
    if (o==OK && cs < TYST_first_final)
        o = TYSTBAD;

    return o;
}
