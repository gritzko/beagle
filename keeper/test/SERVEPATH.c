//  SERVEPATH — WIREServePath: served repo-path + `?/<project>` selector.
//
//  Regression for the submodule-fetch bug where the keeper-peer
//  `?/<project>` selector was dropped on the client side, so a fetch
//  for project `libabc` was served the store's row-0 default project
//  (`dogs`) and never carried the pinned commit.  The selector must
//  ride along on the served path for keeper peers; a bare `?ref`
//  (the want, sent in-band) must NOT.

#include "keeper/WIRE.h"

#include <string.h>

#include "abc/PRO.h"
#include "abc/TEST.h"

//  One row: feed `path` + `query` through WIREServePath, expect `want_s`.
static ok64 check(char const *path, char const *query, char const *want_s) {
    sane(path && want_s);
    a_pad(u8, out, 256);
    a_cstr(path_s, path);
    u8cs query_s = {};
    if (query) { a_cstr(q, query); u8csMv(query_s, q); }

    call(WIREServePath, out, path_s, query_s);

    a_dup(u8c, got, u8bData(out));
    size_t wl = strlen(want_s);
    want($len(got) == (ssize_t)wl);
    want(memcmp(got[0], want_s, wl) == 0);
    done;
}

ok64 SERVEPATHtest() {
    sane(1);

    //  Absolute project selector rides along (the bug: this was lost).
    call(check, "beagle", "/libabc",       "beagle?/libabc");
    call(check, "home/gritzko/beagle/.be", "/libabc",
                "home/gritzko/beagle/.be?/libabc");
    //  Absolute project + branch tail.
    call(check, "store", "/proj/feat",     "store?/proj/feat");

    //  Bare ref (want) is NOT a selector — must not be appended.
    call(check, "beagle", "master",        "beagle");
    call(check, "beagle", "heads/main",    "beagle");

    //  No query at all — path passes through untouched.
    call(check, "beagle", NULL,            "beagle");
    call(check, "/abs/path", NULL,         "/abs/path");

    done;
}

ok64 maintest() {
    sane(1);
    call(SERVEPATHtest);
    done;
}

TEST(maintest)
