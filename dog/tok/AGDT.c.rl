#include "abc/INT.h"
#include "abc/PRO.h"
#include "AGDT.h"

ok64 AGDTonComment (u8cs tok, AGDTstate* state);
ok64 AGDTonString (u8cs tok, AGDTstate* state);
ok64 AGDTonNumber (u8cs tok, AGDTstate* state);
ok64 AGDTonPragma (u8cs tok, AGDTstate* state);
ok64 AGDTonWord (u8cs tok, AGDTstate* state);
ok64 AGDTonPunct (u8cs tok, AGDTstate* state);
ok64 AGDTonSpace (u8cs tok, AGDTstate* state);

%%{

machine AGDT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
dgt = [0-9];
xdgt = [0-9a-fA-F];

# Agda allows almost any non-whitespace in identifiers (unicode-heavy, mixfix)
# We approximate: ASCII letters, digits, underscores, hyphens, primes, and high bytes
idalpha = [a-zA-Z_] | (0x80..0xff);
idalnum = [a-zA-Z_0-9\-'] | (0x80..0xff);

esc = [\\] ( [abefnrtv\\'\"0]
           | [x] xdgt+
           | dgt+ );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_pragma {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonPragma(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = AGDTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- pragmas {-# ... #-} ----
    "{-#" ( any8 - [#] | [#] (any8 - [\-]) | [#] [\-] (any8 - [}]) )* "#-}"  => on_pragma;

    # ---- block comments {- ... -} (flat approximation) ----
    "{-" ( any8 - [\-] | [\-] (any8 - [}]) )* "-}"              => on_comment;

    # ---- line comments ----
    "--" [^\n]*                                                   => on_comment;

    # ---- strings ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- char literals ----
    ['] ( esc | any8 - ['\\] ) [']                                => on_string;

    # ---- numbers ----
    "0" [xX] xdgt+                                                => on_number;
    dgt+ "." dgt+ ([eE] [+\-]? dgt+)?                            => on_number;
    dgt+ [eE] [+\-]? dgt+                                         => on_number;
    dgt+                                                           => on_number;

    # ---- identifiers (including unicode) ----
    idalpha idalnum*                                              => on_word;

    # ---- punctuation ----
    [(){}[\].;:=|\\@!?]                                          => on_punct;
    "->" | "<-" | "::" | "=>" | ".."                              => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 AGDTLexer(AGDTstate* state) {

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
    if (o==OK && cs < AGDT_first_final)
        o = AGDTBAD;

    return o;
}
