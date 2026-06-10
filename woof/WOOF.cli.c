//  woof CLI — thin wrapper: parse, open, exec, close.
//
//  Woof has no shard index of its own and writes nothing under `.be/`
//  — it just opens a TCP socket, accepts clients, and forks worker
//  dogs (keeper / graf / spot / bro) per request.  The repo is opened
//  read-only; workers spawn from woof's own directory via
//  HOMEResolveSibling (CONN.c reads WOOF_ARGV0 to anchor that walk).

#include "WOOF.h"

#include "abc/FILE.h"
#include "abc/PRO.h"
#include "dog/CLI.h"

//  Set by CONN.c; assigned here from argv[0] so worker spawns can
//  reach siblings via HOMEResolveSibling.
extern char const *WOOF_ARGV0;

static ok64 woofcli_inner(cli *c) {
    sane(c);
    call(FILEInit);
    call(CLIParse, c, WOOF_VERBS, WOOF_VAL_FLAGS);

    home h = {};
    call(HOMEOpenAt, &h, $path(c->repo), NO);

    try(WOOFOpen, &h);
    ok64 wo = __;
    if (wo != OK) { HOMEClose(&h); return wo; }

    //  Tell CONN.c which argv[0] to feed HOMEResolveSibling.
    WOOF_ARGV0 = (char const *)$arg(0)[0];

    try(WOOFExec, c);
    ok64 ret = __;

    WOOFClose();
    HOMEClose(&h);
    return ret;
}

ok64 woofcli() {
    sane(1);
    cli c = {};
    call(PATHu8bAlloc, c.repo);
    call(u8csbAlloc, c.flags, CLI_MAX_FLAGS * 2);
    call(u8csbAlloc,  c.uris,  CLI_MAX_URIS);
    try(woofcli_inner, &c);
    ok64 ret = __;
    u8csbFree(c.flags);
    u8csbFree(c.uris);
    PATHu8bFree(c.repo);
    return ret;
}

MAIN(woofcli);
