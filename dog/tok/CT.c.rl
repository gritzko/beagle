#include "abc/INT.h"
#include "abc/PRO.h"
#include "CT.h"

// user functions (callbacks) for the parser
ok64 CTonComment (u8cs tok, CTstate* state);
ok64 CTonString (u8cs tok, CTstate* state);
ok64 CTonNumber (u8cs tok, CTstate* state);
ok64 CTonPreproc (u8cs tok, CTstate* state);
ok64 CTonWord (u8cs tok, CTstate* state);
ok64 CTonPunct (u8cs tok, CTstate* state);
ok64 CTonSpace (u8cs tok, CTstate* state);

%%{

machine CT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_$];
idalnum = [a-zA-Z_$0-9];
dgt = [0-9];
xdgt = [0-9a-fA-F];
odgt = [0-7];

# escape sequences shared by strings and chars
esc = [\\] ( [abefnrtv\\'\"?0]
           | odgt{1,3}
           | [x] xdgt+
           | [u] xdgt{4}
           | [U] xdgt{8} );

# string prefix: L u U u8
strpfx = [LuU] | "u8";

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_preproc {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonPreproc(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = CTonSpace(tok, state);
    if (o!=OK) fbreak;
}

# line continuation (consumed silently inside preproc)
bscont = [\\] [\r]? [\n];

# decimal dgts with optional C23 dgt separators
ddig = dgt+ ( ['] dgt+ )*;
xdig = xdgt+ ( ['] xdgt+ )*;
bdig = [01]+ ( ['] [01]+ )*;

# integer suffixes: u/U, l/L, ll/LL, combinations
isuf = ( [uU] ([lL] [lL]?)? | ([lL] [lL]?) [uU]? )?;

# float suffixes: f F l L
fsuf = [fFlLdD]?;

main := |*

    # ---- comments ----
    "//" [^\n]*                                                   => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- string literals (with optional prefix) ----
    strpfx? ["] ( esc | any8 - ["\\] )* ["]                      => on_string;

    # ---- character literals (with optional prefix) ----
    strpfx? ['] ( esc | any8 - ['\\] )* [']                      => on_string;

    # ---- numbers ----
    # hex float: 0x1.Fp10
    "0" [xX] xdig ("." xdig?)? ([pP] [+\-]? ddig) fsuf          => on_number;
    # hex integer
    "0" [xX] xdig isuf                                           => on_number;
    # binary integer
    "0" [bB] bdig isuf                                           => on_number;
    # octal integer (leading 0, only octal dgts)
    "0" odgt+ isuf                                             => on_number;
    # decimal float with dot: 1.0, .5, 1.
    ddig "." ddig? ([eE] [+\-]? ddig)? fsuf                     => on_number;
    "." ddig ([eE] [+\-]? ddig)? fsuf                            => on_number;
    # decimal with exponent: 1e10
    ddig [eE] [+\-]? ddig fsuf                                   => on_number;
    # plain decimal integer
    ddig isuf                                                     => on_number;

    # ---- preprocessor directive keyword ----
    [#] [ \t]* ("include" | "define" | "undef" |
                "ifdef" | "ifndef" | "if" | "elif" |
                "else" | "endif" |
                "pragma" | "error" | "warning" | "line")          => on_preproc;
    # bare # (e.g. stringify operator, or unknown directive)
    [#] [ \t]* idalpha idalnum*                                   => on_preproc;
    [#]                                                           => on_punct;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    ">>=" | "<<=" | "+=" | "-=" | "*=" | "/=" | "%=" |
    "&=" | "^=" | "|=" | ">>" | "<<" | "++" | "--" |
    "->" | "&&" | "||" | "<=" | ">=" | "==" | "!=" |
    "..."                                                         => on_punct;

    # ---- single-char punctuation (everything else) ----
    (any8 - idalpha - dgt - ws - ["'#] - [.])                   => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

// the public API function
ok64 CTLexer(CTstate* state) {

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
    if (o==OK && cs < CT_first_final)
        o = CTBAD;

    return o;
}
