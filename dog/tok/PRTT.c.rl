#include "abc/INT.h"
#include "abc/PRO.h"
#include "PRTT.h"

ok64 PRTTonComment (u8cs tok, PRTTstate* state);
ok64 PRTTonString (u8cs tok, PRTTstate* state);
ok64 PRTTonNumber (u8cs tok, PRTTstate* state);
ok64 PRTTonWord (u8cs tok, PRTTstate* state);
ok64 PRTTonPunct (u8cs tok, PRTTstate* state);
ok64 PRTTonSpace (u8cs tok, PRTTstate* state);

%%{

machine PRTT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abfnrtv\\'"] | odgt{1,3} | [x] xdgt{2} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRTTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- comments ----
    "//" [^\n]*                                                   => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- double-quoted strings ----
    0x22 ( esc | any8 - [\\] - 0x22 - [\n] )* 0x22              => on_string;

    # ---- single-quoted strings ----
    0x27 ( esc | any8 - [\\] - 0x27 - [\n] )* 0x27              => on_string;

    # ---- numbers ----
    "0" [xX] xdgt+                                                => on_number;
    "0" odgt+                                                     => on_number;
    dgt+ "." dgt* ([eE] [+\-]? dgt+)?                            => on_number;
    "." dgt+ ([eE] [+\-]? dgt+)?                                 => on_number;
    dgt+ ([eE] [+\-]? dgt+)?                                     => on_number;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- punctuation ----
    (any8 - idalpha - dgt - ws - 0x22 - 0x27 - [.])             => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 PRTTLexer(PRTTstate* state) {

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
    if (o==OK && cs < PRTT_first_final)
        o = PRTTBAD;

    return o;
}
