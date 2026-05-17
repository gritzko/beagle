#include "abc/INT.h"
#include "abc/PRO.h"
#include "JST.h"

ok64 JSTonComment (u8cs tok, JSTstate* state);
ok64 JSTonString (u8cs tok, JSTstate* state);
ok64 JSTonNumber (u8cs tok, JSTstate* state);
ok64 JSTonWord (u8cs tok, JSTstate* state);
ok64 JSTonPunct (u8cs tok, JSTstate* state);
ok64 JSTonSpace (u8cs tok, JSTstate* state);

%%{

machine JST;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_$];
idalnum = [a-zA-Z_$0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abefnrtv\\'\"?0/]
           | odgt{1,3}
           | [x] xdgt{2}
           | [u] xdgt{4}
           | [u] "{" xdgt{1,6} "}" );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = JSTonSpace(tok, state);
    if (o!=OK) fbreak;
}

# digit groups with underscore separator
ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
bdig = [01] ( [_]? [01] )*;
odig = odgt ( [_]? odgt )*;

# BigInt suffix
nsuf = [n]?;

main := |*

    # ---- comments ----
    "//" [^\n]*                                                   => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;
    "#!" [^\n]*                                                   => on_comment;

    # ---- template strings (backtick, flat -- no nesting) ----
    0x60 ( [\\] any8 | any8 - 0x60 - [\\] )* 0x60                => on_string;

    # ---- string literals ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;
    ['] ( esc | any8 - ['\\] )* [']                               => on_string;

    # ---- regex literal (approximate: after certain punct/keyword contexts) ----
    # we approximate: /.../ with flags, not starting with * or /
    # Note: this is a simplification; true regex detection needs parser state
    "/" ( [\\] any8 | any8 - [/\\\n] )+ "/" [dgimsuyv]*  => on_string;

    # ---- numbers ----
    "0" [xX] xdig nsuf                                            => on_number;
    "0" [oO] odig nsuf                                            => on_number;
    "0" [bB] bdig nsuf                                            => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)? nsuf                      => on_number;
    "." ddig ([eE] [+\-]? ddig)? nsuf                             => on_number;
    ddig [eE] [+\-]? ddig nsuf                                    => on_number;
    ddig nsuf                                                      => on_number;

    # ---- identifiers (including #private) ----
    [#]? idalpha idalnum*                                         => on_word;

    # ---- multi-char operators ----
    ">>>" | ">>>=" |
    ">>=" | "<<=" | "**=" |
    "+=" | "-=" | "*=" | "/=" | "%=" |
    "&=" | "|=" | "^=" |
    "&&=" | "||=" | "??=" |
    ">>" | "<<" | "**" |
    "++" | "--" | "=>" | "?." | "??" |
    "&&" | "||" | "<=" | ">=" | "==" | "!=" |
    "===" | "!==" | "..."                                         => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'#] - [.] - 0x60)             => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 JSTLexer(JSTstate* state) {

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
    if (o==OK && cs < JST_first_final)
        o = JSTBAD;

    return o;
}
