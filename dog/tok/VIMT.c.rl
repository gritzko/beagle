#include "abc/INT.h"
#include "abc/PRO.h"
#include "VIMT.h"

ok64 VIMTonComment (u8cs tok, VIMTstate* state);
ok64 VIMTonString (u8cs tok, VIMTstate* state);
ok64 VIMTonNumber (u8cs tok, VIMTstate* state);
ok64 VIMTonWord (u8cs tok, VIMTstate* state);
ok64 VIMTonPunct (u8cs tok, VIMTstate* state);
ok64 VIMTonSpace (u8cs tok, VIMTstate* state);

%%{

machine VIMT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];

esc = [\\] ( [abefnrtv\\"\n0]
           | [x] xdgt{2} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = VIMTonSpace(tok, state);
    if (o!=OK) fbreak;
}

ddig = dgt+;
xdig = xdgt+;

main := |*

    # ---- comments: " to end of line (VimScript comment) ----
    ["] [^\n]* [\n]                                               => on_comment;

    # ---- single-quoted strings (literal, no escapes) ----
    ['] ( any8 - ['] )* [']                                       => on_string;

    # ---- numbers ----
    "0" [xX] xdig                                                  => on_number;
    "0" [oO] [0-7]+                                                => on_number;
    "0" [bB] [01]+                                                 => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)?                             => on_number;
    ddig [eE] [+\-]? ddig                                          => on_number;
    ddig                                                            => on_number;

    # ---- identifiers (Vim allows : # in some contexts) ----
    idalpha idalnum* [:#]?                                         => on_word;

    # ---- variable prefixes ----
    [&] idalpha idalnum*                                           => on_word;

    # ---- multi-char operators ----
    "==" | "!=" | ">=" | "<=" | "=~" | "!~" |
    "==?" | "==\x23" | "=~?" | "=~\x23" |
    "&&" | "||" | ".." | "->" |
    "+=" | "-=" | "*=" | "/=" | "%=" | ".="                        => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'&] - [.])                     => on_punct;
    [.]                                                            => on_punct;

    # ---- whitespace ----
    ws+                                                            => on_space;

*|;

}%%

%%write data;

ok64 VIMTLexer(VIMTstate* state) {

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
    if (o==OK && cs < VIMT_first_final)
        o = VIMTBAD;

    return o;
}
