#include "abc/INT.h"
#include "abc/PRO.h"
#include "CMKT.h"

ok64 CMKTonComment (u8cs tok, CMKTstate* state);
ok64 CMKTonString (u8cs tok, CMKTstate* state);
ok64 CMKTonNumber (u8cs tok, CMKTstate* state);
ok64 CMKTonVar (u8cs tok, CMKTstate* state);
ok64 CMKTonWord (u8cs tok, CMKTstate* state);
ok64 CMKTonPunct (u8cs tok, CMKTstate* state);
ok64 CMKTonSpace (u8cs tok, CMKTstate* state);

%%{

machine CMKT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CMKTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CMKTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CMKTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_var {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CMKTonVar(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CMKTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CMKTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CMKTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- comments ----
    [#] [^\n]*                                                    => on_comment;

    # ---- string literals ----
    ["] ( [\\] any8 | any8 - ["\\] )* ["]                        => on_string;

    # ---- variable references ----
    "${" ( any8 - [}] )* "}"                                      => on_var;

    # ---- numbers ----
    dgt+ ("." dgt+)?                                              => on_number;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["#])                            => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 CMKTLexer(CMKTstate* state) {

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
    if (o==OK && cs < CMKT_first_final)
        o = CMKTBAD;

    return o;
}
