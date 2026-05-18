//  CFG — gitconfig-family structural parser.  See CFG.h.  The ragel
//  grammar (CFG.c.rl → CFG.rl.c) provides `CFGu8sFeed`; this file
//  layers `CFGu8sDrain` on top for callers that just want a flat
//  (section, key, value) stream.

#include "CFG.h"

#include "abc/BUF.h"
#include "abc/PATH.h"
#include "abc/PRO.h"

ok64 CFGu8sDrain (u8cs ini, u8bp buf,
                  path8bp section, u8csp key, u8csp val) {
    sane(ini != NULL && buf != NULL && section != NULL &&
         key != NULL && val != NULL);

    CFGstate s = { .data = {ini[0], ini[1]}, .buf = buf };

    //  Loop until an assignment event (non-empty key) or terminal.
    //  Section-change events bubble up only as updates to `section`.
    for (;;) {
        ok64 o = CFGu8sFeed(&s);
        ini[0] = s.data[0];
        if (o != OK) return o;

        if (u8csEmpty(s.key)) {
            //  Section header: refresh the path8b.
            u8bReset(section);
            call(PATHu8bAdd, section, s.sec);
            if (!u8csEmpty(s.sub)) call(PATHu8bAdd, section, s.sub);
            continue;
        }

        key[0] = s.key[0];
        key[1] = s.key[1];
        val[0] = s.value[0];
        val[1] = s.value[1];
        done;
    }
}
