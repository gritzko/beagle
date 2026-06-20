//  MKDTB — the StrictMark block grammar, in 4-char blocks.
//
//  Per wiki/StrictMark the block layer is a regular language: every line's
//  structural prefix is a run of 4-space indent (div) blocks followed by at
//  most one marker, and the whole-line leaf shapes (heading, code fence,
//  ruler, reference definition) are likewise fixed.  This one machine owns all
//  of it — there is no hand-rolled line classification left in MKDT.c.
//
//  A marker is exactly 4 chars wide: a single '>' (quote) or '-' (bullet)
//  padded with spaces in any of the four columns, 1-3 digits then '.' filling
//  the slot (numbered), or a -[ ]/-[v]/-[-]/-[x] todo.  Anything off-grammar (e.g.
//  "-- ", a two-dash run) leaves marker NONE, so the line is a paragraph.  A
//  header needs the gap space the spec mandates ("####" alone is not one); a
//  ruler is 3-4 dashes with a blank rest (the 3-dash short exception); a code
//  fence is a run of >=3 backticks (the wrapper accepts only 3 or 4).
//
//  Fields are set by FINISHING actions (@), which fire on the transition that
//  consumes a token's last byte — so they fire even when the next byte is
//  ordinary content (the common case), unlike leaving actions (%), which do
//  not fire on a dead-end transition.  `content` is set to p+1 (one past the
//  matched byte) = the first content byte.  One line, one classification.
//
//  Build: ragel -C MKDTB.c.rl -o MKDTB.rl.c -L

#include "MKDT.h"
#include "abc/PRO.h"

%%{
    machine mkdtb;
    alphtype unsigned char;

    action indent_end { b->depth += 1; b->content = (const u8c *)(p + 1); }
    action quote_end  { b->marker = MKDT_MARK_QUOTE; b->content = (const u8c *)(p + 1); }
    action ulist_end  { b->marker = MKDT_MARK_ULIST; b->content = (const u8c *)(p + 1); }
    action olist_end  { b->marker = MKDT_MARK_OLIST; b->content = (const u8c *)(p + 1); }
    action todo_end   { b->marker = MKDT_MARK_TODO;  b->content = (const u8c *)(p + 1); }
    action h_start    { hs = p; }
    action h_level    { b->heading = (int)(p - hs); b->content = (const u8c *)(p + 1); }
    action h_space    { b->content = (const u8c *)(p + 1); }
    action f_start    { fs = p; }
    action f_end      { b->fence = (int)(p + 1 - fs); b->content = (const u8c *)(p + 1); }
    action hr_set     { b->hrule = YES; }
    action rd_set     { b->refdef = YES; }

    sp     = ' ';
    ws     = sp | 0x09 | 0x0d;
    nl     = 0x0a;
    indent = sp sp sp sp ;

    quote  = ('>' sp sp sp) | (sp '>' sp sp) | (sp sp '>' sp) | (sp sp sp '>') ;
    bullet = ('-' sp sp sp) | (sp '-' sp sp) | (sp sp '-' sp) | (sp sp sp '-') ;
    dig    = 0x30 .. 0x39 ;
    number = (dig '.' sp sp) | (sp dig '.' sp) | (sp sp dig '.')
           | (dig dig '.' sp) | (sp dig dig '.') | (dig dig dig '.') ;
    #  TODO marker: a dash then a bracketed one-char state, a 4-char block
    #  (-[ ] not started, -[v]/-[V] done, -[-] blocked, -[x]/-[X] wontfix),
    #  then the gap space.  Distinct from a bullet (a lone dash) and a ruler.
    todo   = '-' '[' (sp | 'v' | 'V' | '-' | 'x' | 'X') ']' sp ;

    alnm   = dig | 0x41 .. 0x5a | 0x61 .. 0x7a ;
    refdef = '[' alnm ']' ':' @rd_set ;

    #  ATX header: 1-4 '#' then the mandatory gap space(s); the level is fixed
    #  only once that first gap space is consumed, so "####x" stays a paragraph.
    heading = ('#' {1,4}) >h_start sp @h_level (sp* $h_space) ;

    #  Code fence: a run of >=3 backticks (the wrapper accepts only 3/4).  The
    #  width/content are set by a finishing action so a lone 1-2 backtick run
    #  (inline code at line start) never matches and never touches content; the
    #  blank-vs-info rest is decided in C from the recorded run end.
    fence  = ('`' '`' '`' '`'*) >f_start @f_end ;

    #  Ruler: 3-4 dashes, blank rest, terminating newline (3-dash short form).
    hrule  = ('-' '-' '-' '-'?) ws* nl @hr_set ;

    marker = quote @quote_end | bullet @ulist_end | number @olist_end
           | todo @todo_end ;

    main := (indent @indent_end)*
            ( marker | heading | fence | hrule | refdef )? ;
}%%

%% write data;

void MKDTBlock(u8csc line, mkdtblock *b) {
    b->depth = 0;
    b->marker = MKDT_MARK_NONE;
    b->heading = 0;
    b->fence = 0;
    b->fence_blank = NO;
    b->hrule = NO;
    b->refdef = NO;
    a_dup(u8c, data, line);
    const unsigned char *p = (const unsigned char *)data[0];
    const unsigned char *pe = (const unsigned char *)data[1];
    const unsigned char *eof = pe;
    const unsigned char *hs = p, *fs = p;
    b->content = (const u8c *)p;   // default: content begins after the indents
    int cs;
    %% write init;
    %% write exec;

    //  A fence closes a block iff the rest after its backtick run is blank; an
    //  info string makes it an opener.  `content` is the byte past the run.
    if (b->fence >= 3) {
        b->fence_blank = YES;
        for (const unsigned char *q = (const unsigned char *)b->content; q < pe;
             ++q)
            if (*q != ' ' && *q != '\t' && *q != '\r' && *q != '\n') {
                b->fence_blank = NO;
                break;
            }
    }
    (void)eof; (void)hs; (void)fs;
    (void)mkdtb_en_main; (void)mkdtb_error; (void)mkdtb_first_final;
}
