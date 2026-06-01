//  MARKE — HTML escaper for the mark dog.
//  Feeds `text` into `out`, mapping & < > " to entities; every other run
//  is copied verbatim.  Table-driven so the special-char detection is a
//  ragel scanner, never a hand-rolled byte scan.
//
//  Build: ragel -C MARKE.c.rl -o MARKE.rl.c -L

#include "abc/INT.h"
#include "abc/PRO.h"
#include "MARK.h"

%%{
    machine marke;
    alphtype unsigned char;

    main := |*
        '&'             => { o = MARKu8bLit(out, "&amp;");  if (o != OK) fbreak; };
        '<'             => { o = MARKu8bLit(out, "&lt;");   if (o != OK) fbreak; };
        '>'             => { o = MARKu8bLit(out, "&gt;");   if (o != OK) fbreak; };
        '"'             => { o = MARKu8bLit(out, "&quot;"); if (o != OK) fbreak; };
        (any - [&<>"])+ => { u8cs s = {(u8c *)ts, (u8c *)te};
                             o = u8bFeed(out, s); if (o != OK) fbreak; };
    *|;
}%%

%% write data;

ok64 MARKu8bFeedEsc(u8bp out, u8csc text) {
    sane($ok(text));
    a_dup(u8c, data, text);

    int cs;
    int act = 0;
    u8c *p = (u8c *)data[0];
    u8c *pe = (u8c *)data[1];
    u8c *eof = pe;
    u8c *ts = NULL;
    u8c *te = NULL;
    ok64 o = OK;

    %% write init;
    %% write exec;

    (void)act;
    return o;
}
