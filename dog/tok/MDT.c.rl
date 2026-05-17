#include "abc/INT.h"
#include "abc/PRO.h"
#include "MDT.h"

ok64 MDTonEmph (u8cs tok, MDTstate* state);
ok64 MDTonCode (u8cs tok, MDTstate* state);
ok64 MDTonComment (u8cs tok, MDTstate* state);
ok64 MDTonLink (u8cs tok, MDTstate* state);
ok64 MDTonNumber (u8cs tok, MDTstate* state);
ok64 MDTonWord (u8cs tok, MDTstate* state);
ok64 MDTonPunct (u8cs tok, MDTstate* state);
ok64 MDTonSpace (u8cs tok, MDTstate* state);

%%{

machine MDT;

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
    o = MDTonEmph(tok, state);
    if (o!=OK) fbreak;
}
action on_code {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonCode(tok, state);
    if (o!=OK) fbreak;
}
action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_link {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonLink(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = MDTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- inline code ----
    "``" ( any8 - [`] | [`] (any8 - [`]) )* "``"         => on_code;
    "`" ( any8 - [`\n] )+ "`"                            => on_code;

    # ---- HTML comments (single line) ----
    "<!--" ( any8 - [\-] | [\-] (any8 - [\-]) | "--" (any8 - [>]) )* "-->"  => on_comment;

    # ---- bold **content** (left-flanking: first char non-ws non-*) ----
    "**" (nws - [*]) ( any8 - [*\n] | [*] (any8 - [*\n]) )* "**"  => on_emph;

    # ---- italic *content* (left-flanking) ----
    "*" (nws - [*]) (any8 - [*\n])* "*"                  => on_emph;

    # ---- bold __content__ ----
    "__" (nws - [_]) ( any8 - [_\n] | [_] (any8 - [_\n]) )* "__"  => on_emph;

    # ---- italic _content_ ----
    "_" (nws - [_]) (any8 - [_\n])* "_"                  => on_emph;

    # ---- strikethrough ~~content~~ ----
    "~~" (nws - [~]) ( any8 - [~\n] | [~] (any8 - [~\n]) )* "~~" => on_emph;

    # ---- link/image URLs: (url) ----
    "(" [a-zA-Z] [a-zA-Z0-9+.\-]* "://" [^)\n]+ ")"    => on_link;

    # ---- numbers ----
    "0" [xX] xdgt+                                       => on_number;
    dgt+ "." dgt*                                        => on_number;
    "." dgt+                                             => on_number;
    dgt+                                                 => on_number;

    # ---- bare emphasis delimiters (unmatched) ----
    "**"                                                 => on_punct;
    "__"                                                 => on_punct;
    "~~"                                                 => on_punct;

    # ---- identifiers / words ----
    idalpha idalnum*                                     => on_word;

    # ---- list markers ----
    [\-*+] [ \t]                                         => on_punct;

    # ---- blockquote ----
    [>]                                                  => on_punct;

    # ---- punctuation ----
    [[\]()!*_~|\\:.\-+#&<>{}=,;/'"@^`]                  => on_punct;

    # ---- newlines ----
    nl                                                   => on_space;

    # ---- whitespace ----
    ws+                                                  => on_space;

    # ---- UTF-8 multibyte ----
    (0x80..0xff) (0x80..0xbf)*                           => on_word;

    # ---- catch-all: any byte not matched above ----
    any8                                                 => on_punct;

*|;

}%%

%%write data;

ok64 MDTInlineLexer(MDTstate* state) {

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
    if (o==OK && cs < MDT_first_final)
        o = MDTBAD;

    return o;
}
