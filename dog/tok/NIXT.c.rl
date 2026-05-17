#include "abc/INT.h"
#include "abc/PRO.h"
#include "NIXT.h"

ok64 NIXTonComment (u8cs tok, NIXTstate* state);
ok64 NIXTonString (u8cs tok, NIXTstate* state);
ok64 NIXTonNumber (u8cs tok, NIXTstate* state);
ok64 NIXTonWord (u8cs tok, NIXTstate* state);
ok64 NIXTonPunct (u8cs tok, NIXTstate* state);
ok64 NIXTonSpace (u8cs tok, NIXTstate* state);

%%{

machine NIXT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9\-];
dgt = [0-9];

esc = [\\] ( [nrt\\"$] | "${"  );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = NIXTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- block comments /* ... */ ----
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"           => on_comment;
    # ---- line comments ----
    [#] [^\n]*                                                     => on_comment;

    # ---- indented strings '' ... '' (Nix uses '' for multi-line) ----
    ['] ['] ( any8 - ['] | ['] (any8 - [']) )* ['] [']            => on_string;

    # ---- regular strings ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- paths ----
    [./] [a-zA-Z0-9._\-/]+                                        => on_word;
    [~] "/" [a-zA-Z0-9._\-/]+                                     => on_word;
    "<" [a-zA-Z0-9._\-/]+ ">"                                     => on_string;

    # ---- numbers (integers only in Nix) ----
    dgt+                                                           => on_number;

    # ---- identifiers (Nix allows - in identifiers) ----
    idalpha idalnum*                                               => on_word;

    # ---- multi-char operators ----
    "++" | "==" | "!=" | "<=" | ">=" | "&&" | "||" |
    "->" | "//" | "..." | "?." | ":"                               => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'#./~<] - [.])                => on_punct;

    # ---- whitespace ----
    ws+                                                            => on_space;

*|;

}%%

%%write data;

ok64 NIXTLexer(NIXTstate* state) {

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
    if (o==OK && cs < NIXT_first_final)
        o = NIXTBAD;

    return o;
}
