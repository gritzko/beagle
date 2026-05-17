#include "abc/INT.h"
#include "abc/PRO.h"
#include "LLT.h"

// user functions (callbacks) for the parser
ok64 LLTonComment (u8cs tok, LLTstate* state);
ok64 LLTonString (u8cs tok, LLTstate* state);
ok64 LLTonNumber (u8cs tok, LLTstate* state);
ok64 LLTonWord (u8cs tok, LLTstate* state);
ok64 LLTonPunct (u8cs tok, LLTstate* state);
ok64 LLTonSpace (u8cs tok, LLTstate* state);

%%{

machine LLT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z$_];
idalnum = [a-zA-Z$_.0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];

# escape sequences in strings
esc = [\\] any8;

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = LLTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- comments: ; to end of line ----
    ";" [^\n]*                                                    => on_comment;

    # ---- string literals (with optional c prefix) ----
    [c]? ["] ( esc | any8 - ["\\] )* ["]                          => on_string;

    # ---- numbers ----
    # hex constants (including typed: 0xK... 0xL... 0xM... 0xH...)
    "0" [xX] [KLMH]? xdgt+                                       => on_number;
    # decimal integer
    dgt+                                                          => on_number;
    # decimal float
    dgt+ "." dgt* ([eE] [+\-]? dgt+)?                            => on_number;

    # ---- sigils as punctuation (@global, %local, !metadata, #attrgroup) ----
    [@%!#]                                                        => on_punct;

    # ---- identifiers (letters, _, $; may contain dots and digits) ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char punctuation ----
    "..."                                                         => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - [;"] - [@%!#])                  => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

// the public API function
ok64 LLTLexer(LLTstate* state) {

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
    if (o==OK && cs < LLT_first_final)
        o = LLTBAD;

    return o;
}
