#include "abc/INT.h"
#include "abc/PRO.h"
#include "PRLT.h"

ok64 PRLTonComment (u8cs tok, PRLTstate* state);
ok64 PRLTonString (u8cs tok, PRLTstate* state);
ok64 PRLTonNumber (u8cs tok, PRLTstate* state);
ok64 PRLTonWord (u8cs tok, PRLTstate* state);
ok64 PRLTonPunct (u8cs tok, PRLTstate* state);
ok64 PRLTonSpace (u8cs tok, PRLTstate* state);

%%{

machine PRLT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];

esc = [\\] ( [abefnrtv\\'\"?0\n]
           | [x] xdgt{2}
           | [u] xdgt{4}
           | [N] "{" [a-zA-Z ]+ "}" );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = PRLTonSpace(tok, state);
    if (o!=OK) fbreak;
}

ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
bdig = [01] ( [_]? [01] )*;
odig = [0-7] ( [_]? [0-7] )*;

main := |*

    # ---- comments ----
    [#] [^\n]*                                                     => on_comment;

    # ---- POD documentation (=head1 ... =cut) ----
    "=" ("head1"|"head2"|"head3"|"head4"|"over"|"item"|"back"|"begin"|"end"|"pod"|"for"|"encoding"|"cut") [^\n]*  => on_comment;

    # ---- qw/qq/q quoting ----
    "qw" [({[/|!] ( any8 - [)}\]/|!] )* [)}\]/|!]               => on_string;
    "qq" [({[/|!] ( [\\] any8 | any8 - [)}\]/|!\\] )* [)}\]/|!] => on_string;
    "q"  [({[/|!] ( any8 - [)}\]/|!] )* [)}\]/|!]               => on_string;

    # ---- strings ----
    ["] ( [\\] any8 | any8 - ["\\] )* ["]                        => on_string;
    ['] ( any8 - ['\\] | [\\] any8 )* [']                        => on_string;

    # ---- regex (approximate) ----
    "m" [/] ( [\\] any8 | any8 - [/\\] )* [/] [msixpgce]*        => on_string;
    "s" [/] ( [\\] any8 | any8 - [/\\] )* [/]
            ( [\\] any8 | any8 - [/\\] )* [/] [msixpgce]*        => on_string;

    # ---- numbers ----
    "0" [xX] xdig                                                 => on_number;
    "0" [bB] bdig                                                 => on_number;
    "0" odig                                                       => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)?                            => on_number;
    "." ddig ([eE] [+\-]? ddig)?                                  => on_number;
    ddig [eE] [+\-]? ddig                                         => on_number;
    ddig                                                           => on_number;

    # ---- variables ----
    [$@%] idalpha idalnum*                                        => on_word;
    [$] [_0-9!@&+'./\\,;]                                        => on_word;

    # ---- identifiers ----
    idalpha idalnum* ( "::" idalpha idalnum* )*                   => on_word;

    # ---- multi-char operators ----
    "=>" | "->" | "++" | "--" | "**" | "=~" | "!~" |
    "<=>" | "&&" | "||" | "//" |
    "+=" | "-=" | "*=" | "/=" | "%=" | "**=" |
    "&=" | "|=" | "^=" | "<<=" | ">>=" | "//=" |
    "&&=" | "||=" | ".=" |
    "<<" | ">>" | "<=" | ">=" | "==" | "!=" |
    "eq" | "ne" | "lt" | "gt" | "le" | "ge" |
    ".." | "..."                                                   => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - [#] - [.])                      => on_punct;
    [.]                                                            => on_punct;

    # ---- whitespace ----
    ws+                                                            => on_space;

*|;

}%%

%%write data;

ok64 PRLTLexer(PRLTstate* state) {

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
    if (o==OK && cs < PRLT_first_final)
        o = PRLTBAD;

    return o;
}
