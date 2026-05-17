#include "abc/INT.h"
#include "abc/PRO.h"
#include "SCSST.h"

ok64 SCSSTonComment (u8cs tok, SCSSTstate* state);
ok64 SCSSTonString (u8cs tok, SCSSTstate* state);
ok64 SCSSTonNumber (u8cs tok, SCSSTstate* state);
ok64 SCSSTonAtRule (u8cs tok, SCSSTstate* state);
ok64 SCSSTonWord (u8cs tok, SCSSTstate* state);
ok64 SCSSTonVar (u8cs tok, SCSSTstate* state);
ok64 SCSSTonPunct (u8cs tok, SCSSTstate* state);
ok64 SCSSTonSpace (u8cs tok, SCSSTstate* state);

%%{

machine SCSST;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_\-];
idalnum = [a-zA-Z_\-0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];

esc = [\\] any8;

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_atrule {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonAtRule(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_var {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonVar(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SCSSTonSpace(tok, state);
    if (o!=OK) fbreak;
}

# units
unit = [a-zA-Z%]+;

main := |*

    # ---- comments ----
    "//" [^\n]*                                                   => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- strings ----
    0x22 ( esc | any8 - [\\] - 0x22 )* 0x22                     => on_string;
    0x27 ( esc | any8 - [\\] - 0x27 )* 0x27                     => on_string;

    # ---- hex colors ----
    [#] xdgt{3,8}                                                 => on_number;

    # ---- interpolation #{...} ----
    [#] "{" ( any8 - [}] )* "}"                                  => on_punct;

    # ---- numbers with optional units ----
    dgt+ ("." dgt+)? unit?                                        => on_number;
    "." dgt+ unit?                                                 => on_number;

    # ---- at-rules ----
    "@" [a-zA-Z\-]+                                               => on_atrule;

    # ---- variables ----
    "$" [a-zA-Z_] [a-zA-Z_0-9\-]*                                => on_var;

    # ---- identifiers ----
    [a-zA-Z_] idalnum*                                            => on_word;
    [\-] idalnum+                                                 => on_word;

    # ---- punctuation ----
    (any8 - [a-zA-Z_\-] - dgt - ws - 0x22 - 0x27 - [@$#] - [.]) => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 SCSSTLexer(SCSSTstate* state) {

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
    if (o==OK && cs < SCSST_first_final)
        o = SCSSTBAD;

    return o;
}
