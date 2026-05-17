#include "abc/INT.h"
#include "abc/PRO.h"
#include "LAXT.h"

ok64 LAXTonComment (u8cs tok, LAXTstate* state);
ok64 LAXTonMath (u8cs tok, LAXTstate* state);
ok64 LAXTonCommand (u8cs tok, LAXTstate* state);
ok64 LAXTonNumber (u8cs tok, LAXTstate* state);
ok64 LAXTonWord (u8cs tok, LAXTstate* state);
ok64 LAXTonPunct (u8cs tok, LAXTstate* state);
ok64 LAXTonSpace (u8cs tok, LAXTstate* state);

%%{

machine LAXT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z];
idalnum = [a-zA-Z0-9];
dgt = [0-9];

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_math {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonMath(tok, state);
    if (o!=OK) fbreak;
}
action on_command {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonCommand(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LAXTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- comments (% to end of line) ----
    [%] [^\n]*                                                    => on_comment;

    # ---- display math $$...$$ ----
    "$" "$" ( any8 - [$] | [$] (any8 - [$]) )* "$" "$"          => on_math;

    # ---- inline math $...$ ----
    "$" ( any8 - [$] )+ "$"                                      => on_math;

    # ---- commands \name ----
    [\\] idalpha+                                                 => on_command;

    # ---- escaped special char ----
    [\\] (any8 - idalpha)                                        => on_punct;

    # ---- numbers ----
    dgt+ "." dgt*                                                 => on_number;
    "." dgt+                                                      => on_number;
    dgt+                                                           => on_number;

    # ---- plain text words ----
    idalpha idalnum*                                              => on_word;

    # ---- braces and special chars ----
    [{}[\]&~^_]                                                   => on_punct;

    # ---- other punctuation ----
    (any8 - idalpha - dgt - ws - [%$\\{}[\]&~^_.])              => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 LAXTLexer(LAXTstate* state) {

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
    if (o==OK && cs < LAXT_first_final)
        o = LAXTBAD;

    return o;
}
