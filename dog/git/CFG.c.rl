//  CFG — gitconfig-family structural parser.  See CFG.h.
//
//  Pull-mode ragel grammar.  Each `CFGu8sFeed` call advances the input
//  to the next assignment OR section header, then fbreaks.  Decoded
//  bytes accumulate in the caller-owned scratch buffer:
//
//      PAST          DATA          IDLE
//      [ sec  sub ] [ key  value ] [ ... ]
//
//  sec/sub live in PAST and stick across assigns under the same
//  section.  key/value live in DATA and get recycled (Back) on every
//  fresh assign.  On section emit, DATA→PAST (Bate) so the just-decoded
//  sec/sub become sticky.
//
//  Escape decoding (`\n` `\t` `\b` `\"` `\\`) and backslash-newline
//  value continuation are performed inline; the caller sees decoded
//  bytes via state->{sec,sub,key,value} slices into `buf`.

#include "CFG.h"

#include "abc/B.h"
#include "abc/PRO.h"

static u8 CFGdecEsc (u8 c) {
    switch (c) {
        case 'n':  return '\n';
        case 't':  return '\t';
        case 'b':  return '\b';
        case '"':  return '"';
        case '\\': return '\\';
        default:   return c;     // grammar restricts to the set above
    }
}

%%{

machine CFG;

alphtype unsigned char;

nl    = '\n';
hsp   = [ \t\r];

# Quoted-string content: contiguous literal runs + single-byte escapes.
q_run = (any - [\n"\\])+;
q_esc = '\\' [ntb"\\];

# Bare value content: literal runs (no quote/escape/comment-start) +
# backslash-newline line continuation (joins segments, emits nothing).
b_run  = (any - [\n"\\#;])+;
b_cont = '\\' nl;

# Comment tail: recognised so it doesn't break parsing; not emitted.
ctail = [#;] (any - nl)*;

# Section/key character sets per gitconfig spec.
sn_ch = [A-Za-z0-9.\-];
kn_1  = [A-Za-z];
kn_n  = [A-Za-z0-9\-];

# --------------------------------------------------------------------
# Actions.  `run_s` is a parser-local pointer marking the start of the
# current literal run; on run-close we feed `[run_s, p)` to the buffer.
# `o` is the parser-local return code; setting it before `fbreak`
# surfaces an error to the caller.
# --------------------------------------------------------------------

action mk_run         { run_s = p; }

# Section name (no escapes; sn_ch only).
action sec_clear      { u8bReset(state->buf); }
action sec_open       { state->sec[0] = (u8c *)state->buf[2]; }
action sec_run_done   {
    u8cs r = {run_s, p};
    if (u8bFeed(state->buf, r) != OK) { o = CFGNOBUF; fbreak; }
}
action sec_close      { state->sec[1] = (u8c *)state->buf[2]; }

# Quoted subsection name (with escape decoding).
action sub_open       { state->sub[0] = (u8c *)state->buf[2]; }
action sub_run_done   {
    u8cs r = {run_s, p};
    if (u8bFeed(state->buf, r) != OK) { o = CFGNOBUF; fbreak; }
}
action sub_esc_done   {
    if (u8bFeed1(state->buf, CFGdecEsc(p[-1])) != OK) {
        o = CFGNOBUF; fbreak;
    }
}
action sub_close      { state->sub[1] = (u8c *)state->buf[2]; }

action emit_section   {
    Bate(state->buf);                                   // DATA → PAST
    state->key[0]   = state->key[1]   = (u8c *)state->buf[2];
    state->value[0] = state->value[1] = (u8c *)state->buf[2];
    emitted = YES;
    //  Leaving-action: `%emit_section` fires on the transition AWAY
    //  from `section_hdr`, which happens AFTER the trailing `\n` is
    //  already consumed.  `*p` at this point is the first char of
    //  the next line, not the `\n`.  `fbreak`'s built-in `p++` would
    //  skip that char.  Exit without advancing p.
    {goto _out;}
}

# Assignment start: drop the previous assign's DATA but keep PAST
# (sec/sub) intact.  Pre-initialise key/value to empty so a `key =`
# line with no value still produces a well-formed event.
action ass_open       {
    Back(state->buf);
    state->key[0]   = state->key[1]   = (u8c *)state->buf[2];
    state->value[0] = state->value[1] = (u8c *)state->buf[2];
}

action key_open       { state->key[0] = (u8c *)state->buf[2]; }
action key_run_done   {
    u8cs r = {run_s, p};
    if (u8bFeed(state->buf, r) != OK) { o = CFGNOBUF; fbreak; }
}
action key_close      { state->key[1] = (u8c *)state->buf[2]; }

# Quoted value (escapes decoded, no trailing-ws trim).
action val_q_open     { state->value[0] = (u8c *)state->buf[2]; }
action val_q_run_done {
    u8cs r = {run_s, p};
    if (u8bFeed(state->buf, r) != OK) { o = CFGNOBUF; fbreak; }
}
action val_q_esc_done {
    if (u8bFeed1(state->buf, CFGdecEsc(p[-1])) != OK) {
        o = CFGNOBUF; fbreak;
    }
}
action val_q_close    { state->value[1] = (u8c *)state->buf[2]; }

# Bare value (literal runs joined by line continuation; surrounding
# whitespace stripped at close — see note in val_b_close).
action val_b_open     { state->value[0] = (u8c *)state->buf[2]; }
action val_b_run_done {
    u8cs r = {run_s, p};
    if (u8bFeed(state->buf, r) != OK) { o = CFGNOBUF; fbreak; }
}
action val_b_close    {
    //  Trim trailing whitespace.  The DATA cursor `buf[2]` defines
    //  where the value ends; rewind past any trailing sp/tab/cr.
    u8c **bb = (u8c **)state->buf;
    while (bb[2] > bb[1] &&
           (bb[2][-1] == ' ' || bb[2][-1] == '\t' || bb[2][-1] == '\r'))
        bb[2]--;
    //  Trim leading whitespace.  Ragel's DFA resolves the ambiguity
    //  between `hsp*` after `=` and `bvalue`'s `b_run` (which also
    //  matches space) by letting `b_run` win — so leading spaces
    //  land inside the value.  Advance state->value[0] past them
    //  before sealing.
    while (state->value[0] < (u8c const *)bb[2] &&
           (state->value[0][0] == ' '  ||
            state->value[0][0] == '\t' ||
            state->value[0][0] == '\r'))
        state->value[0]++;
    state->value[1] = (u8c *)state->buf[2];
}

action emit_assign    {
    //  See emit_section: leaving-action, no p++ to avoid over-advance.
    emitted = YES;
    {goto _out;}
}

# --------------------------------------------------------------------
# Grammar.
# --------------------------------------------------------------------

subname = '"' >sub_open
          ( q_run >mk_run %sub_run_done
          | q_esc         %sub_esc_done )*
          '"' %sub_close;

section_hdr = hsp* '[' >sec_clear hsp*
              ( sn_ch+ >sec_open >mk_run %sec_run_done %sec_close )
              ( hsp+ subname )?
              hsp* ']' hsp* ctail? nl
              %emit_section;

qvalue = '"' >val_q_open
         ( q_run >mk_run %val_q_run_done
         | q_esc         %val_q_esc_done )*
         '"' %val_q_close;

bvalue = ( b_run >mk_run %val_b_run_done
         | b_cont                         )+
         >val_b_open %val_b_close;

value_form = qvalue | bvalue;

key_part = ( kn_1 kn_n* ) >key_open >mk_run %key_run_done %key_close;

assign_line = hsp* >ass_open
              key_part
              hsp* '=' hsp*
              value_form?
              hsp* ctail? nl
              %emit_assign;

comment_line = hsp* ctail nl;
blank_line   = hsp* nl;

line  = section_hdr | assign_line | comment_line | blank_line;
main := line*;

}%%

%%write data;

ok64 CFGu8sFeed (CFGstate *state) {
    sane(state != NULL && $ok(state->data) && Bok(state->buf));

    int  cs  = 0;
    u8c *p   = (u8c *)state->data[0];
    u8c *pe  = (u8c *)state->data[1];
    u8c *eof = pe;
    u8c const *run_s = NULL;
    ok64 o = OK;
    //  `emit_section` / `emit_assign` set this before `fbreak`.  After
    //  `fbreak`, `cs` is whatever ragel's transition table picked for
    //  the post-nl state — `line*` doesn't necessarily fold back into
    //  `CFG_first_final` immediately, so the cs-based check below is
    //  not enough to distinguish a successful event from a real
    //  parse error.  The flag lets us return OK without consulting cs.
    b8 emitted = NO;

    %% write init;
    %% write exec;

    state->data[0] = p;
    if (o != OK) return o;
    if (emitted) return OK;
    if (p == pe && cs >= CFG_first_final) return NODATA;
    if (cs < CFG_first_final) return CFGBAD;
    return OK;
}
