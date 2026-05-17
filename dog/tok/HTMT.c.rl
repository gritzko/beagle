#include "abc/INT.h"
#include "abc/PRO.h"
#include "HTMT.h"

ok64 HTMTonComment (u8cs tok, HTMTstate* state);
ok64 HTMTonString (u8cs tok, HTMTstate* state);
ok64 HTMTonTag (u8cs tok, HTMTstate* state);
ok64 HTMTonPunct (u8cs tok, HTMTstate* state);
ok64 HTMTonText (u8cs tok, HTMTstate* state);
ok64 HTMTonSpace (u8cs tok, HTMTstate* state);

%%{

machine HTMT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9\-];
dgt = [0-9];
xdgt = [0-9a-fA-F];

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_tag {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonTag(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_text {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonText(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = HTMTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- HTML comments ----
    "<!--" ( any8 - [\-] | [\-] (any8 - [\-]) | [\-] [\-] (any8 - [>]) )* "-->"  => on_comment;

    # ---- DOCTYPE ----
    "<!" [Dd] [Oo] [Cc] [Tt] [Yy] [Pp] [Ee] [^\n>]* ">"        => on_tag;

    # ---- closing tags ----
    "</" idalpha idalnum* ">"                                     => on_tag;

    # ---- self-closing tags ----
    "<" idalpha idalnum* [^>]* "/>"                               => on_tag;

    # ---- opening tag start (just the tag name) ----
    "<" idalpha idalnum*                                          => on_tag;

    # ---- attribute strings ----
    ["] ( any8 - ["] )* ["]                                       => on_string;
    ['] ( any8 - ['] )* [']                                       => on_string;

    # ---- tag close ----
    "/>" | ">"                                                    => on_punct;

    # ---- entities ----
    "&" ( [a-zA-Z]+ | [#] dgt+ | [#] [xX] xdgt+ ) ";"           => on_punct;

    # ---- identifiers (attribute names etc) ----
    idalpha idalnum*                                              => on_text;

    # ---- punctuation ----
    [=<>/]                                                        => on_punct;

    # ---- plain text (anything not a tag) ----
    (any8 - [<>&"'=] - ws - idalpha)+                            => on_text;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 HTMTLexer(HTMTstate* state) {

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
    if (o==OK && cs < HTMT_first_final)
        o = HTMTBAD;

    return o;
}
