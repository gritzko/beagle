#include "abc/INT.h"
#include "abc/PRO.h"
#include "CLJT.h"

ok64 CLJTonComment (u8cs tok, CLJTstate* state);
ok64 CLJTonString (u8cs tok, CLJTstate* state);
ok64 CLJTonNumber (u8cs tok, CLJTstate* state);
ok64 CLJTonWord (u8cs tok, CLJTstate* state);
ok64 CLJTonPunct (u8cs tok, CLJTstate* state);
ok64 CLJTonSpace (u8cs tok, CLJTstate* state);

%%{

machine CLJT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

# Clojure symbols can contain special chars
symc = [a-zA-Z_0-9!\?\-\*\+\/\.><=%&];
symalpha = [a-zA-Z_!\?\*\+\-\/><=%&];

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CLJTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- comments ----
    ";" [^\n]*                                                    => on_comment;
    "#_"                                                          => on_comment;

    # ---- string literals ----
    ["] ( [\\] any8 | any8 - ["\\] )* ["]                        => on_string;

    # ---- regex literals ----
    "#" ["] ( [\\] any8 | any8 - ["\\] )* ["]                    => on_string;

    # ---- numbers ----
    # hex
    "0" [xX] xdgt+                                               => on_number;
    # octal
    "0" odgt+                                                     => on_number;
    # rational
    dgt+ "/" dgt+                                                 => on_number;
    # float with BigDecimal M
    dgt+ "." dgt* ([eE] [+\-]? dgt+)? [MN]?                     => on_number;
    "." dgt+ ([eE] [+\-]? dgt+)? [MN]?                           => on_number;
    # decimal with exponent
    dgt+ [eE] [+\-]? dgt+ [MN]?                                  => on_number;
    # plain integer with optional BigInt N
    dgt+ [MN]?                                                    => on_number;

    # ---- keywords (colon-prefixed) ----
    ":" symc+                                                     => on_string;

    # ---- identifiers/symbols ----
    symalpha symc*                                                => on_word;

    # ---- reader macros and punctuation ----
    "#(" | "#{" | "#'" | "^" | "@" | "'" | 0x60 | "~@" | "~"    => on_punct;

    # ---- single-char punctuation ----
    (any8 - symalpha - dgt - ws - [";:] - [.#])                  => on_punct;
    [.#]                                                          => on_punct;

    # ---- whitespace (commas are whitespace in Clojure) ----
    (ws | [,])+                                                   => on_space;

*|;

}%%

%%write data;

ok64 CLJTLexer(CLJTstate* state) {

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
    if (o==OK && cs < CLJT_first_final)
        o = CLJTBAD;

    return o;
}
