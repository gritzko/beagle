#include "abc/INT.h"
#include "abc/PRO.h"
#include "YMLT.h"

ok64 YMLTonComment (u8cs tok, YMLTstate* state);
ok64 YMLTonString (u8cs tok, YMLTstate* state);
ok64 YMLTonNumber (u8cs tok, YMLTstate* state);
ok64 YMLTonWord (u8cs tok, YMLTstate* state);
ok64 YMLTonPunct (u8cs tok, YMLTstate* state);
ok64 YMLTonSpace (u8cs tok, YMLTstate* state);

%%{

machine YMLT;

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
    o = YMLTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = YMLTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- comments ----
    [#] [^\n]*                                                    => on_comment;

    # ---- double-quoted strings ----
    0x22 ( [\\] any8 | any8 - [\\] - 0x22 )* 0x22               => on_string;

    # ---- single-quoted strings (no escapes) ----
    0x27 ( any8 - 0x27 )* 0x27                                   => on_string;

    # ---- numbers ----
    "0" [xX] xdgt+                                                => on_number;
    "0" [oO] odgt+                                                => on_number;
    [\-+]? "." ("inf" | "Inf" | "INF")                            => on_number;
    "." ("nan" | "NaN" | "NAN")                                   => on_number;
    [\-+]? dgt+ "." dgt* ([eE] [+\-]? dgt+)?                     => on_number;
    [\-+]? "." dgt+ ([eE] [+\-]? dgt+)?                          => on_number;
    [\-+]? dgt+ ([eE] [+\-]? dgt+)?                              => on_number;

    # ---- anchors and aliases ----
    [&*] idalpha idalnum*                                         => on_punct;

    # ---- tags ----
    "!" idalnum+                                                  => on_punct;
    "!!" idalnum+                                                 => on_punct;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- block scalar indicators ----
    [|>]                                                          => on_punct;

    # ---- punctuation ----
    (any8 - idalpha - dgt - ws - [#] - [.])                     => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 YMLTLexer(YMLTstate* state) {

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
    if (o==OK && cs < YMLT_first_final)
        o = YMLTBAD;

    return o;
}
