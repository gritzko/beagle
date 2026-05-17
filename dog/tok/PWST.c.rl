#include "abc/INT.h"
#include "abc/PRO.h"
#include "PWST.h"

ok64 PWSTonComment (u8cs tok, PWSTstate* state);
ok64 PWSTonString (u8cs tok, PWSTstate* state);
ok64 PWSTonNumber (u8cs tok, PWSTstate* state);
ok64 PWSTonVar (u8cs tok, PWSTstate* state);
ok64 PWSTonWord (u8cs tok, PWSTstate* state);
ok64 PWSTonPunct (u8cs tok, PWSTstate* state);
ok64 PWSTonSpace (u8cs tok, PWSTstate* state);

%%{

machine PWST;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_var {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonVar(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PWSTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- comments ----
    [#] [^\n]*                                                    => on_comment;
    "<#" ( any8 - [#] | [#]+ (any8 - [#>]) )* [#]+ ">"          => on_comment;

    # ---- here-strings ----
    "@" ["] ( any8 - ["] | ["] (any8 - [@]) )* ["] "@"           => on_string;
    "@" ['] ( any8 - ['] | ['] (any8 - [@]) )* ['] "@"           => on_string;

    # ---- string literals ----
    ["] ( [\\] any8 | 0x60 any8 | any8 - ["\\] - 0x60 )* ["]    => on_string;
    ['] ( [']['] | any8 - ['] )* [']                              => on_string;

    # ---- variables ----
    "$" idalpha idalnum*                                          => on_var;
    "$" "{" ( any8 - [}] )* "}"                                  => on_var;

    # ---- numbers ----
    "0" [xX] xdgt+                                                => on_number;
    dgt+ "." dgt+ ([eE] [+\-]? dgt+)?                            => on_number;
    "." dgt+ ([eE] [+\-]? dgt+)?                                  => on_number;
    dgt+ [eE] [+\-]? dgt+                                         => on_number;
    dgt+                                                           => on_number;

    # ---- identifiers (including cmdlet names with dash) ----
    idalpha idalnum* ( [\-] idalpha idalnum* )*                  => on_word;

    # ---- multi-char operators ----
    "+=" | "-=" | "*=" | "/=" | "%=" |
    "++" | "--" |
    "-eq" | "-ne" | "-lt" | "-gt" | "-le" | "-ge" |
    "-and" | "-or" | "-not" | "-band" | "-bor" | "-bxor" |
    "-match" | "-notmatch" | "-like" | "-notlike" |
    "-contains" | "-notcontains" |
    "-replace" | "-split" | "-join" |
    "=>" | "|" | "||" | "&&" | ">>" | ">=" | "<=" | "==" | "!=" => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'#$@] - [.])                  => on_punct;
    [.@]                                                          => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 PWSTLexer(PWSTstate* state) {

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
    if (o==OK && cs < PWST_first_final)
        o = PWSTBAD;

    return o;
}
