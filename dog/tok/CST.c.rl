#include "abc/INT.h"
#include "abc/PRO.h"
#include "CST.h"

ok64 CSTonComment (u8cs tok, CSTstate* state);
ok64 CSTonString (u8cs tok, CSTstate* state);
ok64 CSTonNumber (u8cs tok, CSTstate* state);
ok64 CSTonPreproc (u8cs tok, CSTstate* state);
ok64 CSTonWord (u8cs tok, CSTstate* state);
ok64 CSTonPunct (u8cs tok, CSTstate* state);
ok64 CSTonSpace (u8cs tok, CSTstate* state);

%%{

machine CST;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_@];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];

esc = [\\] ( [abefnrtv\\'\"?0]
           | [x] xdgt{1,4}
           | [u] xdgt{4}
           | [U] xdgt{8} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_preproc {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonPreproc(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSTonSpace(tok, state);
    if (o!=OK) fbreak;
}

ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
bdig = [01] ( [_]? [01] )*;

isuf = ( [uU]? [lL]{0,2} | [lL]{0,2} [uU]? )?;
fsuf = [fFdDmM]?;

main := |*

    # ---- comments ----
    "///" [^\n]*                                                  => on_comment;
    "//" [^\n]*                                                   => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- raw string literals (triple-quoted) ----
    ["] ["] ["] ( any8 - ["] | ["] (any8 - ["]) | ["] ["] (any8 - ["]) )* ["] ["] ["]   => on_string;

    # ---- verbatim strings @"..." ----
    "@" ["] ( ["] ["] | any8 - ["] )* ["]                        => on_string;

    # ---- interpolated strings $"..." ----
    "$" ["] ( esc | any8 - ["\\] )* ["]                          => on_string;

    # ---- regular strings ----
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

    # ---- preprocessor ----
    [#] [ \t]* ("if" | "elif" | "else" | "endif" |
                "define" | "undef" | "region" | "endregion" |
                "nullable" | "pragma" | "error" | "warning" |
                "line")                                           => on_preproc;
    [#] [ \t]* [a-zA-Z_] [a-zA-Z_0-9]*                           => on_preproc;
    [#]                                                           => on_punct;

    # ---- identifiers (@ prefix for verbatim identifiers) ----
    "@" [a-zA-Z_] [a-zA-Z_0-9]*                                  => on_word;
    [a-zA-Z_] [a-zA-Z_0-9]*                                      => on_word;

    # ---- multi-char operators ----
    "?." | "??" | "??=" | "=>" | "::" |
    ">>=" | "<<=" |
    "+=" | "-=" | "*=" | "/=" | "%=" |
    "&=" | "|=" | "^=" |
    ">>" | "<<" | "++" | "--" |
    "->" | "&&" | "||" | "<=" | ">=" | "==" | "!=" |
    "..."                                                         => on_punct;

    # ---- single-char punctuation ----
    (any8 - [a-zA-Z_@] - dgt - ws - ["'#] - [.])                => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 CSTLexer(CSTstate* state) {

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
    if (o==OK && cs < CST_first_final)
        o = CSTBAD;

    return o;
}
