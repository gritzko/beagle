//  woof/ROUTE.c — be-URI scheme → worker binary dispatch table.
//
//  Lookup is a linear scan over a 10-row table; one cache line.  No
//  point in a hash.  Adding a new projector dog = one row + a binary
//  to fork+exec.  The verbless catch-all sits at the end so explicit
//  schemes always win.

#include "WOOF.h"

#include <string.h>

//  Every accepted scheme dispatches to `be` — it's the canonical
//  entry point that already composes `--at <root>?<branch>#<sha>`
//  from `<root>/.be/wtlog` and forwards `--tlv` to the right worker
//  (graf for log/diff/head, keeper for blob/tree/commit, spot for
//  search projectors, bro for verbless paths).  Woof's role here is
//  HTTP framing + scheme allow-listing, not dog selection.
//
//  The scheme rows are still distinct entries so a 404 fires cleanly
//  on schemes outside this set (the table is the read-only safelist).

//  One row per read-only projector scheme (the safelist).  Mirrors
//  dog/DOG.c's DOG_PROJECTORS so every projection `be` can serve is
//  reachable over HTTP; the binary is always `be` for the fork path,
//  while `--api` mode resolves the owning dog via DOGProjectorDog and
//  dispatches in-process.  Keep these two tables in sync.
woof_route const WOOF_ROUTES[] = {
    //  keeper-owned views.
    { u8slit("sha1"),   u8slit("be") },
    { u8slit("blob"),   u8slit("be") },
    { u8slit("tree"),   u8slit("be") },
    { u8slit("commit"), u8slit("be") },
    { u8slit("refs"),   u8slit("be") },
    { u8slit("size"),   u8slit("be") },
    { u8slit("type"),   u8slit("be") },

    //  graf-owned views.
    { u8slit("log"),    u8slit("be") },
    { u8slit("diff"),   u8slit("be") },
    { u8slit("head"),   u8slit("be") },
    { u8slit("blame"),  u8slit("be") },
    { u8slit("weave"),  u8slit("be") },
    { u8slit("map"),    u8slit("be") },

    //  sniff-owned views (worktree).
    { u8slit("ls"),     u8slit("be") },
    { u8slit("lsr"),    u8slit("be") },
    { u8slit("cat"),    u8slit("be") },
    { u8slit("status"), u8slit("be") },

    //  spot-owned views.
    { u8slit("spot"),   u8slit("be") },
    { u8slit("grep"),   u8slit("be") },
    { u8slit("regex"),  u8slit("be") },

    //  Verbless catch-all (bro pager / dir listing).  Must be last.
    { u8slit(""),       u8slit("be") },

    //  Terminator — empty binary slice.
    { u8slit(""),       u8slit("") },
};

woof_route const *WOOFRouteFind(u8cs scheme) {
    for (u32 i = 0; ; i++) {
        woof_route const *r = &WOOF_ROUTES[i];
        if ($empty(r->binary)) return NULL;
        if ($size(r->scheme) == $size(scheme)
            && ($size(scheme) == 0
                || memcmp(*r->scheme, *scheme, $size(scheme)) == 0)) {
            return r;
        }
    }
}
