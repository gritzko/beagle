#include "abc/INT.h"
#include "abc/PRO.h"
#include "DKFT.h"

ok64 DKFTonComment (u8cs tok, DKFTstate* state);
ok64 DKFTonString (u8cs tok, DKFTstate* state);
ok64 DKFTonNumber (u8cs tok, DKFTstate* state);
ok64 DKFTonVar (u8cs tok, DKFTstate* state);
ok64 DKFTonWord (u8cs tok, DKFTstate* state);
ok64 DKFTonPunct (u8cs tok, DKFTstate* state);
ok64 DKFTonSpace (u8cs tok, DKFTstate* state);

%%{

machine DKFT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_var {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonVar(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DKFTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- comments ----
    [#] [^\n]*                                                    => on_comment;

    # ---- string literals ----
    ["] ( [\\] any8 | any8 - ["\\] )* ["]                        => on_string;
    ['] ( any8 - ['] )* [']                                       => on_string;

    # ---- variable references ----
    "${" ( any8 - [}] )* "}"                                      => on_var;
    "$" idalpha idalnum*                                          => on_var;

    # ---- numbers ----
    dgt+ ("." dgt+)?                                              => on_number;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'#$] - [.])                   => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 DKFTLexer(DKFTstate* state) {

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
    if (o==OK && cs < DKFT_first_final)
        o = DKFTBAD;

    return o;
}
