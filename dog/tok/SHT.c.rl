#include "abc/INT.h"
#include "abc/PRO.h"
#include "SHT.h"

ok64 SHTonComment (u8cs tok, SHTstate* state);
ok64 SHTonString (u8cs tok, SHTstate* state);
ok64 SHTonNumber (u8cs tok, SHTstate* state);
ok64 SHTonWord (u8cs tok, SHTstate* state);
ok64 SHTonVar (u8cs tok, SHTstate* state);
ok64 SHTonPunct (u8cs tok, SHTstate* state);
ok64 SHTonSpace (u8cs tok, SHTstate* state);

%%{

machine SHT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SHTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SHTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SHTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SHTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_var {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SHTonVar(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SHTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SHTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- comments ----
    [#] [^\n]*                                                    => on_comment;

    # ---- single-quoted strings (no escapes; absorb '\'' shell quote-toggle) ----
    ['] ( "'\\''" | any8 - ['] )* [']                             => on_string;

    # ---- double-quoted strings ----
    ["] ( [\\] any8 | any8 - ["\\] )* ["]                        => on_string;

    # ---- ANSI-C quoting ----
    "$'" ( [\\] any8 | any8 - ['\\] )* [']                       => on_string;

    # ---- numbers ----
    dgt+                                                           => on_number;
    "0" [xX] xdgt+                                                => on_number;

    # ---- variable expansions ----
    "$" idalpha idalnum*                                          => on_var;
    "$" [?!#$@*\-0-9]                                             => on_var;

    # ---- identifiers/words ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    "$(" | "${" |
    "||" | "&&" | ";;" | ";&" | ";;&" |
    "|&" | ">>" | ">&" | "<&" | "<<" | "<>" |
    "<<<" | "((" | "))" | "[[" | "]]" |
    "+=" | "-=" | "==" | "!=" |
    "-eq" | "-ne" | "-lt" | "-gt" | "-le" | "-ge" |
    "-f" | "-d" | "-e" | "-r" | "-w" | "-x" |
    "-z" | "-n" | "-s"                                            => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - [#] - [.])                      => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 SHTLexer(SHTstate* state) {

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
    if (o==OK && cs < SHT_first_final)
        o = SHTBAD;

    return o;
}
