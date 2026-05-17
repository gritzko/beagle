#include "abc/INT.h"
#include "abc/PRO.h"
#include "FORT.h"

ok64 FORTonComment (u8cs tok, FORTstate* state);
ok64 FORTonString (u8cs tok, FORTstate* state);
ok64 FORTonNumber (u8cs tok, FORTstate* state);
ok64 FORTonWord (u8cs tok, FORTstate* state);
ok64 FORTonPunct (u8cs tok, FORTstate* state);
ok64 FORTonSpace (u8cs tok, FORTstate* state);

%%{

machine FORT;

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
    o = FORTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = FORTonSpace(tok, state);
    if (o!=OK) fbreak;
}

ddig = dgt+;

main := |*

    # ---- comments (! to end of line) ----
    [!] [^\n]*                                                    => on_comment;

    # ---- string literals ----
    ["] ( any8 - ["] )* ["]                                       => on_string;
    ['] ( any8 - ['] )* [']                                       => on_string;

    # ---- numbers ----
    # float with dot
    ddig "." ddig? ([eEdD] [+\-]? ddig)?                         => on_number;
    "." ddig ([eEdD] [+\-]? ddig)?                                => on_number;
    # with exponent
    ddig [eEdD] [+\-]? ddig                                       => on_number;
    # kind specifier
    ddig "_" idalnum+                                             => on_number;
    # plain integer
    ddig                                                           => on_number;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    "==" | "/=" | "<=" | ">=" | "//" | "**" | "::" |
    ".eq." | ".ne." | ".lt." | ".gt." | ".le." | ".ge." |
    ".and." | ".or." | ".not." | ".eqv." | ".neqv." |
    ".true." | ".false."                                          => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'!] - [.])                    => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 FORTLexer(FORTstate* state) {

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
    if (o==OK && cs < FORT_first_final)
        o = FORTBAD;

    return o;
}
