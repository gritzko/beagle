#include "abc/INT.h"
#include "abc/PRO.h"
#include "DT.h"

ok64 DTonComment (u8cs tok, DTstate* state);
ok64 DTonString (u8cs tok, DTstate* state);
ok64 DTonNumber (u8cs tok, DTstate* state);
ok64 DTonWord (u8cs tok, DTstate* state);
ok64 DTonPunct (u8cs tok, DTstate* state);
ok64 DTonSpace (u8cs tok, DTstate* state);

%%{

machine DT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

esc = [\\] ( [abefnrtv\\'\"?0]
           | odgt{1,3}
           | [x] xdgt{2}
           | [u] xdgt{4}
           | [U] xdgt{8} );

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = DTonSpace(tok, state);
    if (o!=OK) fbreak;
}

# digit groups with underscore separator
ddig = dgt ( [_]? dgt )*;
xdig = xdgt ( [_]? xdgt )*;
bdig = [01] ( [_]? [01] )*;

# integer suffixes
isuf = ( [uU] | [L] | "uL" | "Lu" )?;

# float suffixes
fsuf = [fFL]?;

main := |*

    # ---- line comments ----
    "//" [^\n]*                                                   => on_comment;

    # ---- block comments ----
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- nesting comments /+ ... +/ (simplified: no nesting) ----
    "/+" ( any8 - [+] | [+]+ (any8 - [+/]) )* [+]+ "/"          => on_comment;

    # ---- raw strings (backtick) ----
    0x60 ( any8 - 0x60 )* 0x60                                   => on_string;

    # ---- regular string literals ----
    ["] ( esc | any8 - ["\\] )* ["]                               => on_string;

    # ---- wysiwyg/raw string r"..." ----
    [r] ["] ( any8 - ["] )* ["]                                   => on_string;

    # ---- character literals ----
    ['] ( esc | any8 - ['\\] )* [']                               => on_string;

    # ---- numbers ----
    "0" [xX] xdig ("." xdig?)? ([pP] [+\-]? ddig)? fsuf         => on_number;
    "0" [xX] xdig isuf                                            => on_number;
    "0" [bB] bdig isuf                                            => on_number;
    ddig "." ddig? ([eE] [+\-]? ddig)? fsuf                      => on_number;
    "." ddig ([eE] [+\-]? ddig)? fsuf                             => on_number;
    ddig [eE] [+\-]? ddig fsuf                                    => on_number;
    ddig isuf                                                      => on_number;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    ">>=" | "<<=" | ">>>=" |
    "+=" | "-=" | "*=" | "/=" | "%=" |
    "&=" | "|=" | "^=" | "~=" |
    ">>>" | ">>" | "<<" |
    "++" | "--" | ".." |
    "&&" | "||" | "<=" | ">=" | "==" | "!=" |
    "=>"                                                          => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - ["'] - [.] - 0x60)              => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 DTLexer(DTstate* state) {

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
    if (o==OK && cs < DT_first_final)
        o = DTBAD;

    return o;
}
