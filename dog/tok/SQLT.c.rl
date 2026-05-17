#include "abc/INT.h"
#include "abc/PRO.h"
#include "SQLT.h"

ok64 SQLTonComment (u8cs tok, SQLTstate* state);
ok64 SQLTonString (u8cs tok, SQLTstate* state);
ok64 SQLTonNumber (u8cs tok, SQLTstate* state);
ok64 SQLTonWord (u8cs tok, SQLTstate* state);
ok64 SQLTonPunct (u8cs tok, SQLTstate* state);
ok64 SQLTonSpace (u8cs tok, SQLTstate* state);

%%{

machine SQLT;

alphtype unsigned char;

any8 = (0x00..0xff);
ws = [ \t\r\n\f\v];
idalpha = [a-zA-Z_];
idalnum = [a-zA-Z_0-9];
dgt = [0-9];

action on_comment {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonComment(tok, state);
    if (o!=OK) fbreak;
}
action on_string {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonString(tok, state);
    if (o!=OK) fbreak;
}
action on_number {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonNumber(tok, state);
    if (o!=OK) fbreak;
}
action on_word {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonWord(tok, state);
    if (o!=OK) fbreak;
}
action on_punct {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonPunct(tok, state);
    if (o!=OK) fbreak;
}
action on_space {
    tok[0] = (u8c*)ts;
    tok[1] = (u8c*)te;
    o = SQLTonSpace(tok, state);
    if (o!=OK) fbreak;
}

main := |*

    # ---- comments ----
    "--" [^\n]*                                                   => on_comment;
    "/*" ( any8 - [*] | [*]+ (any8 - [*/]) )* [*]+ "/"          => on_comment;

    # ---- single-quoted strings ('' for escape) ----
    0x27 ( 0x27 0x27 | any8 - 0x27 )* 0x27                      => on_string;

    # ---- double-quoted identifiers ----
    0x22 ( any8 - 0x22 )* 0x22                                   => on_string;

    # ---- numbers ----
    dgt+ "." dgt* ([eE] [+\-]? dgt+)?                            => on_number;
    "." dgt+ ([eE] [+\-]? dgt+)?                                 => on_number;
    dgt+ ([eE] [+\-]? dgt+)?                                     => on_number;

    # ---- identifiers ----
    idalpha idalnum*                                              => on_word;

    # ---- multi-char operators ----
    "<=" | ">=" | "<>" | "!=" | "||" | "::"                      => on_punct;

    # ---- single-char punctuation ----
    (any8 - idalpha - dgt - ws - 0x22 - 0x27 - [.])             => on_punct;
    [.]                                                           => on_punct;

    # ---- whitespace ----
    ws+                                                           => on_space;

*|;

}%%

%%write data;

ok64 SQLTLexer(SQLTstate* state) {

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
    if (o==OK && cs < SQLT_first_final)
        o = SQLTBAD;

    return o;
}
