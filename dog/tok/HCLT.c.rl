#include "abc/INT.h"
#include "abc/PRO.h"
#include "HCLT.h"

ok64 HCLTonComment (u8cs tok, HCLTstate* state);
ok64 HCLTonString (u8cs tok, HCLTstate* state);
ok64 HCLTonNumber (u8cs tok, HCLTstate* state);
ok64 HCLTonWord (u8cs tok, HCLTstate* state);
ok64 HCLTonPunct (u8cs tok, HCLTstate* state);
ok64 HCLTonSpace (u8cs tok, HCLTstate* state);

%%{

machine HCLT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];

esc = [\\] ( [nrt"\\] | [u] [0-9a-fA-F]{4} | [U] [0-9a-fA-F]{8} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HCLTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- comments ----
    [#] [^\n]*                                                    => on_comment;
    "//" [^\n]*                                                   => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- strings (with ${} interpolation treated flat) ----
    0x22 ( esc | "${" | any8 - [\\] - 0x22 )* 0x22              => on_string;

    # ---- numbers ----
    dgt+ "." dgt* ([eE] [+\-]? dgt+)?                            => on_number;
    "." dgt+ ([eE] [+\-]? dgt+)?                                 => on_number;
    dgt+ ([eE] [+\-]? dgt+)?                                     => on_number;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    "==" | "!=" | "<=" | ">=" | "&&" | "||" | "=>" | "..."      => on_punct;

    # ---- punctuation ----
    (any8 - idalpha - dgt - ws - [#] - 0x22 - [.])              => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 HCLTLexer(HCLTstate* state) {

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
    if (o==OK && cs < HCLT_first_final)
        o = HCLTBAD;

    return o;
}
