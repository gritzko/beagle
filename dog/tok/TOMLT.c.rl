#include "abc/INT.h"
#include "abc/PRO.h"
#include "TOMLT.h"

ok64 TOMLTonComment (u8cs tok, TOMLTstate* state);
ok64 TOMLTonString (u8cs tok, TOMLTstate* state);
ok64 TOMLTonNumber (u8cs tok, TOMLTstate* state);
ok64 TOMLTonHeader (u8cs tok, TOMLTstate* state);
ok64 TOMLTonWord (u8cs tok, TOMLTstate* state);
ok64 TOMLTonPunct (u8cs tok, TOMLTstate* state);
ok64 TOMLTonSpace (u8cs tok, TOMLTstate* state);

%%{

machine TOMLT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9\-];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];
bdgt = [01];

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_header {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonHeader(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = TOMLTonSpace(tok, state);
    if (o!=OK) fbreak;
}

esc = [\\] ( [btnfr"\\] | [u] xdgt{4} | [U] xdgt{8} );

# TOML digit groups with underscore separator
ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
odig = odgt ( [_]? odgt )*;
bdig = bdgt ( [_]? bdgt )*;

main := |*

    # ---- comments ----
    [#] [^\n]*                                                    => on_comment;

    # ---- multi-line basic strings ----
    0x22 0x22 0x22 ( esc | any8 - [\\] - 0x22 | 0x22{1,2} (any8 - 0x22) )* 0x22{3,5} => on_string;

    # ---- multi-line literal strings ----
    0x27 0x27 0x27 ( any8 - 0x27 | 0x27{1,2} (any8 - 0x27) )* 0x27{3,5} => on_string;

    # ---- basic strings ----
    0x22 ( esc | any8 - [\\] - 0x22 - [\n] )* 0x22              => on_string;

    # ---- literal strings ----
    0x27 ( any8 - 0x27 - [\n] )* 0x27                            => on_string;

    # ---- numbers ----
    [+\-]? "0" [xX] xdig                                         => on_number;
    [+\-]? "0" [oO] odig                                         => on_number;
    [+\-]? "0" [bB] bdig                                         => on_number;
    [+\-]? ("inf" | "nan")                                        => on_number;
    [+\-]? ddig "." ddig ([eE] [+\-]? ddig)?                     => on_number;
    [+\-]? ddig [eE] [+\-]? ddig                                 => on_number;
    [+\-]? ddig                                                   => on_number;

    # ---- date/time (treat as word) ----
    dgt{4} [\-] dgt{2} [\-] dgt{2} ([T ] dgt{2} ":" dgt{2} (":" dgt{2} ("." dgt+)?)? ([Zz] | [+\-] dgt{2} ":" dgt{2})?)? => on_word;

    # ---- table headers [key] and [[key]] ----
    "[[" ( idalnum | [.\-] )+ "]]"                                => on_header;
    "[" ( idalnum | [.\-] )+ "]"                                  => on_header;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- punctuation ----
    (any8 - idalpha - dgt - ws - [#] - 0x22 - 0x27 - [.])       => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 TOMLTLexer(TOMLTstate* state) {

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
    if (o==OK && cs < TOMLT_first_final)
        o = TOMLTBAD;

    return o;
}
