#include "abc/INT.h"
#include "abc/PRO.h"
#include "GQLT.h"

ok64 GQLTonComment (u8cs tok, GQLTstate* state);
ok64 GQLTonString (u8cs tok, GQLTstate* state);
ok64 GQLTonNumber (u8cs tok, GQLTstate* state);
ok64 GQLTonWord (u8cs tok, GQLTstate* state);
ok64 GQLTonPunct (u8cs tok, GQLTstate* state);
ok64 GQLTonSpace (u8cs tok, GQLTstate* state);

%%{

machine GQLT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];

esc = [\\] ( ["\\/bfnrt] | [u] [0-9a-fA-F]{4} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = GQLTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- comments ----
    [#] [^\n]*                                                    => on_comment;

    # ---- block strings ----
    0x22 0x22 0x22 ( any8 - 0x22 | 0x22{1,2} (any8 - 0x22) )* 0x22{3,5} => on_string;

    # ---- strings ----
    0x22 ( esc | any8 - [\\] - 0x22 - [\n] )* 0x22              => on_string;

    # ---- numbers ----
    [\-]? dgt+ "." dgt* ([eE] [+\-]? dgt+)?                     => on_number;
    [\-]? "." dgt+ ([eE] [+\-]? dgt+)?                          => on_number;
    [\-]? dgt+ ([eE] [+\-]? dgt+)?                              => on_number;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- spread operator ----
    "..."                                                         => on_punct;

    # ---- punctuation ----
    (any8 - idalpha - dgt - ws - [#] - 0x22 - [.])              => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 GQLTLexer(GQLTstate* state) {

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
    if (o==OK && cs < GQLT_first_final)
        o = GQLTBAD;

    return o;
}
