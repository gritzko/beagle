//  bro CLI — thin wrapper: parse, open, exec, close.
//
#include "BRO.h"

#include "abc/FILE.h"
#include "abc/PRO.h"
#include "dog/CLI.h"

static ok64 brocli_inner(cli *c) {
    sane(c);
    call(FILEInit);
    call(CLIParse, c, NULL, NULL);  // no verbs, no val-flags
    CLISetHUNKMode(c);

    home h = {};
    call(HOMEOpenAt, &h, $path(c->repo), NO);

    bro b = {};
    try(BROOpen, &b, &h, NO);
    then try(BROExec, &b, c);
    BROClose(&b);
    HOMEClose(&h);
    done;
}

ok64 brocli() {
    sane(1);
    cli c = {};
    call(PATHu8bAlloc, c.repo);
    call(u8csbAlloc, c.flags, CLI_MAX_FLAGS * 2);
    call(uribAlloc,  c.uris,  CLI_MAX_URIS);
    try(brocli_inner, &c);
    u8csbFree(c.flags);
    uribFree(c.uris);
    PATHu8bFree(c.repo);
    done;
}

MAIN(brocli);
