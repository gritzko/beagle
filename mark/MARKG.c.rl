//  MARKG — inline G-token decomposer for the mark dog.
//  MKDT emits emphasis / link / image as whole-span 'G' tokens; this
//  ragel machine splits one into (kind, text, label) so the renderer can
//  emit <strong>/<em>/<del>/<a>/<img> without a hand-rolled scan.
//
//  Forms: *strong*  _emph_  ~~del~~  [text text][l]  [page]  ![alt][l]
//  The explicit label l stays one symbol; a shortcut [page] carries no label,
//  so the renderer keys it on the bracket text.
//
//  Build: ragel -C MARKG.c.rl -o MARKG.rl.c -L

#include "abc/INT.h"
#include "abc/PRO.h"
#include "MARK.h"

%%{
    machine markg;
    alphtype unsigned char;

    action ts0 { txt0 = (u8c *)fpc; }
    action ts1 { txt1 = (u8c *)fpc; }
    action lb0 { lbl0 = (u8c *)fpc; }
    action lb1 { lbl1 = (u8c *)fpc; }
    action k_strong { kind = 'B'; }
    action k_emph   { kind = 'I'; }
    action k_strike { kind = 'D'; }
    action k_link   { kind = 'A'; }
    action k_image  { kind = 'M'; }

    strong  = '*'  ( (any - [*\n])* )  >ts0 %ts1 '*'  @k_strong ;
    emph    = '_'  ( (any - [_\n])* )  >ts0 %ts1 '_'  @k_emph ;
    strike  = '~~' ( (any - [~\n] | '~' (any - [~\n]))* ) >ts0 %ts1 '~~' @k_strike ;
    reflink = '['  ( (any - [\]\n])* ) >ts0 %ts1 '][' ( [0-9A-Za-z] >lb0 %lb1 ) ']' @k_link ;
    shrlink = '['  ( (any - [\]\n])* ) >ts0 %ts1 ']' @k_link ;
    image   = '![' ( (any - [\]\n])* ) >ts0 %ts1 '][' ( [0-9A-Za-z] >lb0 %lb1 ) ']' @k_image ;

    main := strong | emph | strike | reflink | shrlink | image ;
}%%

%% write data;

ok64 MARKDecomposeG(markg *g, u8csc tok) {
    a_dup(u8c, data, tok);

    int cs;
    u8c *p = (u8c *)data[0];
    u8c *pe = (u8c *)data[1];
    u8c *eof = pe;
    u8c *txt0 = NULL, *txt1 = NULL, *lbl0 = NULL, *lbl1 = NULL;
    u8 kind = 0;

    %% write init;
    %% write exec;

    if (cs < markg_first_final) {
        g->kind = 0;
        return OK;
    }
    g->kind = kind;
    g->text[0] = txt0 ? txt0 : (u8c *)data[1];
    g->text[1] = txt1 ? txt1 : (u8c *)data[1];
    if (lbl0 != NULL) {
        g->label[0] = lbl0;
        g->label[1] = lbl1;
    } else {
        //  shortcut: key the link on its bracket text
        g->label[0] = g->text[0];
        g->label[1] = g->text[1];
    }
    return OK;
}
