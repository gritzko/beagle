#include "abc/INT.h"
#include "abc/PRO.h"
#include "CSST.h"

ok64 CSSTonComment (u8cs tok, CSSTstate* state);
ok64 CSSTonString (u8cs tok, CSSTstate* state);
ok64 CSSTonNumber (u8cs tok, CSSTstate* state);
ok64 CSSTonAtRule (u8cs tok, CSSTstate* state);
ok64 CSSTonWord (u8cs tok, CSSTstate* state);
ok64 CSSTonPunct (u8cs tok, CSSTstate* state);
ok64 CSSTonSpace (u8cs tok, CSSTstate* state);

%%{

machine CSST;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_\-];
idalnum = [a-zA-Z_\-0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];

esc = [\\] any8;

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSSTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSSTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSSTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_atrule {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSSTonAtRule(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSSTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSSTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CSSTonSpace(tok, state);
    if (o!=OK) fbreak;
}

# units
unit = [a-zA-Z%]+;

main := |*

    # ---- comments ----
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- strings ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;
    ['] ( esc | any8 - ['\\] )* [']                               => on_string;

    # ---- numbers with optional units ----
    [#] xdgt{3,8}                                                 => on_number;
    dgt+ ("." dgt+)? unit?                                        => on_number;
    "." dgt+ unit?                                                 => on_number;

    # ---- at-rules ----
    "@" [a-zA-Z\-]+                                               => on_atrule;

    # ---- identifiers (selectors, properties, values) ----
    [a-zA-Z_] idalnum*                                            => on_word;
    [\-] idalnum+                                                 => on_word;

    # ---- punctuation ----
    (any8 - [a-zA-Z_\-] - dgt - ws - ["'@] - [.])               => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 CSSTLexer(CSSTstate* state) {

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
    if (o==OK && cs < CSST_first_final)
        o = CSSTBAD;

    return o;
}
