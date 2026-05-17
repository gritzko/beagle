#include "abc/INT.h"
#include "abc/PRO.h"
#include "GLST.h"

ok64 GLSTonComment (u8cs tok, GLSTstate* state);
ok64 GLSTonNumber (u8cs tok, GLSTstate* state);
ok64 GLSTonPreproc (u8cs tok, GLSTstate* state);
ok64 GLSTonWord (u8cs tok, GLSTstate* state);
ok64 GLSTonPunct (u8cs tok, GLSTstate* state);
ok64 GLSTonSpace (u8cs tok, GLSTstate* state);

%%{

machine GLST;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_preproc {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonPreproc(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GLSTonSpace(tok, state);
    if (o!=OK) fbreak;
}

ddig = dgt+;
xdig = xdgt+;

main := |*

    # ---- comments ----
    "//" [^\n]*                                                   => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- preprocessor directives ----
    [#] [ \t]* ("version" | "define" | "undef" |
                "if" | "ifdef" | "ifndef" | "elif" |
                "else" | "endif" |
                "error" | "pragma" | "extension" | "line")        => on_preproc;
    [#] [ \t]* idalpha idalnum*                                   => on_preproc;

    # ---- numbers ----
    "0" [xX] xdig                                                 => on_number;
    "0" odgt+                                                      => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)?                            => on_number;
    "." ddig ([eE] [+\-]? ddig)?                                  => on_number;
    ddig [eE] [+\-]? ddig                                         => on_number;
    ddig                                                           => on_number;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    "<<=" | ">>=" | "+=" | "-=" | "*=" | "/=" | "%=" |
    "&=" | "^=" | "|=" | ">>" | "<<" | "++" | "--" |
    "&&" | "||" | "<=" | ">=" | "==" | "!="                      => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - [#] - [.])                      => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 GLSTLexer(GLSTstate* state) {

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
    if (o==OK && cs < GLST_first_final)
        o = GLSTBAD;

    return o;
}
