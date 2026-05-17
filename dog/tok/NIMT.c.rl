#include "abc/INT.h"
#include "abc/PRO.h"
#include "NIMT.h"

ok64 NIMTonComment (u8cs tok, NIMTstate* state);
ok64 NIMTonString (u8cs tok, NIMTstate* state);
ok64 NIMTonNumber (u8cs tok, NIMTstate* state);
ok64 NIMTonWord (u8cs tok, NIMTstate* state);
ok64 NIMTonPunct (u8cs tok, NIMTstate* state);
ok64 NIMTonSpace (u8cs tok, NIMTstate* state);

%%{

machine NIMT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];

esc = [\\] ( [abcefnlrtv\\'\"?0\n]
           | [x] xdgt{2}
           | dgt{1,3} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIMTonSpace(tok, state);
    if (o!=OK) fbreak;
}

ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
bdig = [01] ( [_]? [01] )*;
odig = [0-7] ( [_]? [0-7] )*;

# type suffixes for numbers
tsuf = ( ['] ( "i8" | "i16" | "i32" | "i64" | "u" | "u8" | "u16" | "u32" | "u64" | "f" | "f32" | "f64" | "d" ) )?;

main := |*

    # ---- block comments #[ ... ]# ----
    "#[" ( any8 - [\]] | [\]] (any8 - [#]) )* "]#"                => on_comment;
    # ---- line comments ----
    [#] [^\n]*                                                     => on_comment;

    # ---- triple-quoted raw strings ----
    [rR]? ["] ["] ["] ( any8 - ["] | ["] (any8 - ["]) | ["] ["] (any8 - ["]) )* ["] ["] ["]  => on_string;

    # ---- raw strings ----
    [rR] ["] ( any8 - ["] )* ["]                                  => on_string;

    # ---- regular strings ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- char literals ----
    ['] ( esc | any8 - ['\\] ) [']                                => on_string;

    # ---- numbers ----
    "0" [xX] xdig tsuf                                             => on_number;
    "0" [oO] odig tsuf                                             => on_number;
    "0" [bB] bdig tsuf                                             => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)? tsuf                       => on_number;
    ddig [eE] [+\-]? ddig tsuf                                    => on_number;
    ddig tsuf                                                      => on_number;

    # ---- identifiers ----
    idalpha idalnum*                                               => on_word;

    # ---- multi-char operators ----
    ".." | "..." | "==" | "!=" | "<=" | ">=" |
    "<<" | ">>" | "+=" | "-=" | "*=" | "/=" |
    "&=" | "|=" | "^=" | "**" | "->" | "=>" |
    "and" | "or" | "not" | "xor" | "shl" | "shr" |
    "div" | "mod"                                                  => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'#] - [.])                    => on_punct;
    [.]                                                            => on_punct;

    # ---- whitespace ----
    ws+                                                            => on_space;

*|;

}%%

%%write data;

ok64 NIMTLexer(NIMTstate* state) {

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
    if (o==OK && cs < NIMT_first_final)
        o = NIMTBAD;

    return o;
}
