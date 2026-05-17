#include "abc/INT.h"
#include "abc/PRO.h"
#include "RST.h"

ok64 RSTonComment (u8cs tok, RSTstate* state);
ok64 RSTonString (u8cs tok, RSTstate* state);
ok64 RSTonNumber (u8cs tok, RSTstate* state);
ok64 RSTonAttr (u8cs tok, RSTstate* state);
ok64 RSTonWord (u8cs tok, RSTstate* state);
ok64 RSTonPunct (u8cs tok, RSTstate* state);
ok64 RSTonSpace (u8cs tok, RSTstate* state);

%%{

machine RST;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abefnrtv\\'\"0\\]
           | [x] xdgt{2}
           | [u] "{" xdgt{1,6} "}"
           | [\n] );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_attr {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonAttr(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RSTonSpace(tok, state);
    if (o!=OK) fbreak;
}

# digit groups with underscore separator
ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
odig = odgt ( [_]? odgt )*;
bdig = [01] ( [_]? [01] )*;

# type suffixes for numbers
typsuf = ( [uif] ("8" | "16" | "32" | "64" | "128" | "size") )?;

main := |*

    # ---- doc comments ----
    "///" [^\n]*                                                  => on_comment;
    "//!" [^\n]*                                                  => on_comment;

    # ---- line comments ----
    "//" [^\n]*                                                   => on_comment;

    # ---- block comments (not nested for simplicity) ----
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- raw strings r#"..."# (up to 3 hashes) ----
    [b]? "r###" ["] ( any8 - ["] | ["] (any8 - [#]) | ["] [#] (any8 - [#]) | ["] [#] [#] (any8 - [#]) )* ["] "###"  => on_string;
    [b]? "r##" ["] ( any8 - ["] | ["] (any8 - [#]) | ["] [#] (any8 - [#]) )* ["] "##"   => on_string;
    [b]? "r#" ["] ( any8 - ["] | ["] (any8 - [#]) )* ["] [#]    => on_string;
    [b]? "r" ["] ( any8 - ["] )* ["]                             => on_string;

    # ---- byte strings ----
    [b] ["] ( esc | any8 - ["\\] )* ["]                          => on_string;

    # ---- regular strings ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- char/byte char literals ----
    [b]? ['] ( esc | any8 - ['\\] ) [']                          => on_string;

    # ---- lifetime (treat as identifier, not string) ----
    ['] idalpha idalnum*                                          => on_word;

    # ---- numbers ----
    "0" [xX] xdig typsuf                                         => on_number;
    "0" [oO] odig typsuf                                         => on_number;
    "0" [bB] bdig typsuf                                         => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)? typsuf                    => on_number;
    ddig [eE] [+\-]? ddig typsuf                                  => on_number;
    ddig typsuf                                                    => on_number;

    # ---- attributes ----
    "#" "!"? "[" idalpha idalnum*                                 => on_attr;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    "..=" | ".." | "=>" | "->" | "::" |
    ">>=" | "<<=" |
    "+=" | "-=" | "*=" | "/=" | "%=" |
    "&=" | "|=" | "^=" |
    ">>" | "<<" |
    "&&" | "||" | "<=" | ">=" | "==" | "!="                      => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - [.])                            => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 RSTLexer(RSTstate* state) {

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
    if (o==OK && cs < RST_first_final)
        o = RSTBAD;

    return o;
}
