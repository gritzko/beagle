#include "abc/INT.h"
#include "abc/PRO.h"
#include "MKDT.h"

ok64 MKDTonEmph (u8cs tok, MKDTstate* state);
ok64 MKDTonCode (u8cs tok, MKDTstate* state);
ok64 MKDTonLink (u8cs tok, MKDTstate* state);
ok64 MKDTonNumber (u8cs tok, MKDTstate* state);
ok64 MKDTonWord (u8cs tok, MKDTstate* state);
ok64 MKDTonPunct (u8cs tok, MKDTstate* state);
ok64 MKDTonSpace (u8cs tok, MKDTstate* state);

%%{

machine MKDT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\f\v];
nl = [\n];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
nws = any8 - [ \t\r\n\f\v];

action on_emph {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonEmph(tok, state);
    if (o!=OK) fbreak;
}
action on_code {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonCode(tok, state);
    if (o!=OK) fbreak;
}
action on_link {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonLink(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MKDTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- inline code `content` (highest precedence) ----
    "`" ( any8 - [`\n] )+ "`"                            => on_code;

    # ---- strong *content* (single star, no doubles) ----
    "*" (nws - [*]) (any8 - [*\n])* "*"                  => on_emph;

    # ---- emph _content_ (single underscore) ----
    "_" (nws - [_]) (any8 - [_\n])* "_"                  => on_emph;

    # ---- strikethrough ~~content~~ ----
    "~~" (nws - [~]) ( any8 - [~\n] | [~] (any8 - [~\n]) )* "~~" => on_emph;

    # ---- reference link [text][x] ----
    "[" (any8 - [\]\n])+ "][" [0-9A-Za-z] "]"            => on_link;

    # ---- image/transclusion ![text][x] ----
    "![" (any8 - [\]\n])+ "][" [0-9A-Za-z] "]"           => on_link;

    # ---- numbers ----
    "0" [xX] xdgt+                                       => on_number;
    dgt+ "." dgt*                                        => on_number;
    "." dgt+                                             => on_number;
    dgt+                                                 => on_number;

    # ---- unmatched delimiters ----
    "~~"                                                 => on_punct;

    # ---- identifiers / words ----
    idalpha idalnum*                                     => on_word;

    # ---- punctuation ----
    [[\]()!*_~|\\:.\-+#&<>{}=,;/'"@^`]                  => on_punct;

    # ---- newlines ----
    nl                                                   => on_space;

    # ---- whitespace ----
    ws+                                                  => on_space;

    # ---- UTF-8 multibyte ----
    (0x80..0xff) (0x80..0xbf)*                           => on_word;

*|;

}%%

%%write data;

ok64 MKDTInlineLexer(MKDTstate* state) {

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
    if (o==OK && cs < MKDT_first_final)
        o = MKDTBAD;

    return o;
}
