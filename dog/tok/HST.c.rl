#include "abc/INT.h"
#include "abc/PRO.h"
#include "HST.h"

ok64 HSTonComment (u8cs tok, HSTstate* state);
ok64 HSTonString (u8cs tok, HSTstate* state);
ok64 HSTonNumber (u8cs tok, HSTstate* state);
ok64 HSTonPragma (u8cs tok, HSTstate* state);
ok64 HSTonWord (u8cs tok, HSTstate* state);
ok64 HSTonPunct (u8cs tok, HSTstate* state);
ok64 HSTonSpace (u8cs tok, HSTstate* state);

%%{

machine HST;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abefnrtv\\\x27\x220&]
           | [x] xdgt+
           | [o] odgt+
           | dgt+ );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_pragma {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonPragma(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HSTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- block comments (not nested for simplicity) ----
    "{-" ( any8 - 0x2d | 0x2d (any8 - 0x7d) )* "-}"             => on_comment;

    # ---- line comments ----
    "--" [^\n]*                                                   => on_comment;

    # ---- strings ----
    0x22 ( [\\] any8 | any8 - 0x22 - [\\] )* 0x22               => on_string;

    # ---- char literals ----
    0x27 ( [\\] any8 | any8 - 0x27 - [\\] ) 0x27                 => on_string;

    # ---- numbers ----
    "0" [xX] xdgt+                                                => on_number;
    "0" [oO] odgt+                                                => on_number;
    "0" [bB] [01]+                                                 => on_number;
    dgt+ "." dgt+ ([eE] [+\-]? dgt+)?                            => on_number;
    dgt+ [eE] [+\-]? dgt+                                         => on_number;
    dgt+                                                           => on_number;

    # ---- identifiers (with tick suffix for primed names) ----
    idalpha ( idalnum | 0x27 )*                                   => on_word;

    # ---- multi-char operators ----
    "::" | "=>" | "->" | "<-" | ".." |
    ">>" | "<<" | "&&" | "||" |
    "<=" | ">=" | "==" | "/="                                     => on_punct;

    # ---- backtick infix ----
    0x60 idalpha idalnum* 0x60                                    => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - 0x22 - 0x27 - 0x60)            => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 HSTLexer(HSTstate* state) {

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
    if (o==OK && cs < HST_first_final)
        o = HSTBAD;

    return o;
}
