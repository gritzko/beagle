#include "abc/INT.h"
#include "abc/PRO.h"
#include "RBT.h"

ok64 RBTonComment (u8cs tok, RBTstate* state);
ok64 RBTonString (u8cs tok, RBTstate* state);
ok64 RBTonNumber (u8cs tok, RBTstate* state);
ok64 RBTonWord (u8cs tok, RBTstate* state);
ok64 RBTonPunct (u8cs tok, RBTstate* state);
ok64 RBTonSpace (u8cs tok, RBTstate* state);

%%{

machine RBT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abefnrstv\\'\"0\\]
           | [x] xdgt{2}
           | [u] xdgt{4}
           | [u] "{" xdgt{1,6} "}"
           | [\n] );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = RBTonSpace(tok, state);
    if (o!=OK) fbreak;
}

# digit groups with underscore separator
ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
odig = odgt ( [_]? odgt )*;
bdig = [01] ( [_]? [01] )*;

main := |*

    # ---- comments ----
    [#] [^\n]*                                                    => on_comment;
    # =begin...=end block comments
    "=begin" [^\n]* [\n] ( any8 - [=] | [=] (any8 - [e]) | "=e" (any8 - [n]) | "=en" (any8 - [d]) )* "=end" [^\n]* => on_comment;

    # ---- strings ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;
    ['] ( any8 - ['\\] | [\\] any8 )* [']                        => on_string;

    # ---- regex literals ----
    "/" ( [\\] any8 | any8 - [/\\] )+ "/" [imxo]*                 => on_string;

    # ---- symbols ----
    ":" idalpha idalnum* [?!]?                                    => on_string;
    ":" ["] ( esc | any8 - ["\\] )* ["]                           => on_string;

    # ---- numbers ----
    "0" [xX] xdig                                                 => on_number;
    "0" [oO] odig                                                 => on_number;
    "0" [bB] bdig                                                 => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)? [ri]?                     => on_number;
    "." ddig ([eE] [+\-]? ddig)? [ri]?                            => on_number;
    ddig [eE] [+\-]? ddig [ri]?                                   => on_number;
    ddig [ri]?                                                     => on_number;

    # ---- identifiers (with ? and ! suffixes) ----
    [@@] idalpha idalnum*                                         => on_word;
    [$] idalpha idalnum*                                          => on_word;
    [$] [!@&+0-9~=/\\,;.<>*$?:\"']                               => on_word;
    idalpha idalnum* [?!]?                                        => on_word;

    # ---- multi-char operators ----
    "<=>" | "=~" | "!~" | "**" | ".." | "..." |
    "&." | "||=" | "&&=" |
    "<<" | ">>" | "+=" | "-=" | "*=" | "/=" | "%=" |
    "&=" | "|=" | "^=" | "**=" | "<<=" | ">>=" |
    "&&" | "||" | "<=" | ">=" | "==" | "!=" |
    "=>" | "->"                                                   => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'#:@$] - [.])                 => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 RBTLexer(RBTstate* state) {

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
    if (o==OK && cs < RBT_first_final)
        o = RBTBAD;

    return o;
}
