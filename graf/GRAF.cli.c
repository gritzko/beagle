//  graf CLI — thin wrapper: parse, open, exec, close.
//
#include "GRAF.h"

#include "abc/FILE.h"
#include "abc/PRO.h"
#include "dog/CLI.h"

//  Verbs that mutate `.be/` (write new graf index runs or touch the
//  DAG).  Everything else — diff, log, head, blame, weave, get,
//  status, map, plus projector schemes resolved via DOGProjectorDog
//  → "graf" — opens read-only and skips the leaf flock so a busy
//  HTTP front (woof) can serve concurrent reads without serialising
//  through LOCK_EX.
static b8 graf_verb_is_rw(u8cs verb) {
    a_cstr(v_index, "index");
    a_cstr(v_merge, "merge");
    return $eq(verb, v_index) || $eq(verb, v_merge);
}

static ok64 grafcli_inner(cli *c) {
    sane(c);
    call(FILEInit);
    call(CLIParse, c, GRAF_CLI_VERBS, GRAF_CLI_VAL_FLAGS);
    CLISetHUNKMode(c);

    //  Lockless opens for read-only verbs; otherwise LOCK_EX on the
    //  leaf dir (writers serialise, including mkdir -p of fresh
    //  branch shards).  See keeper/KEEP.c §KEEPOpenBranch — both
    //  layers gate flock on `rw`.
    b8 rw = graf_verb_is_rw(c->verb);

    //  Prefer `--at` from be; fall back to cwd-walk via c.repo.
    home h = {};
    uri at = {};
    CLIAtURI(&at, c);
    if (u8csEmpty(at.path) && u8bHasData(c->repo))
        u8csMv(at.path, $path(c->repo));
    //  Direct call so we can run HOMEClose on failure: HOMEOpen may
    //  have allocated buffers (root/wt/cur_branch/cur_sha/branches_data)
    //  before the HOMEFindDogs walk-up returned NOHOME.
    {
        ok64 ho = HOMEOpen(&h, &at, rw);
        if (ho != OK) { HOMEClose(&h); return ho; }
    }

    //  Match sniff's branch-aware open: derive cur branch from
    //  `h.cur_branch` (populated by HOMEOpen from `--at <root>?<br>`)
    //  so reindex / lookup walks land at the right shard.
    a_dup(u8c, gbr, u8bDataC(h.cur_branch));
    call(GRAFOpenBranch, &h, gbr, rw);
    ok64 ret = GRAFExec(c);
    GRAFClose();
    HOMEClose(&h);
    return ret;
}

ok64 grafcli() {
    sane(1);
    cli c = {};
    call(PATHu8bAlloc, c.repo);
    call(u8csbAlloc, c.flags, CLI_MAX_FLAGS * 2);
    call(uribAlloc,  c.uris,  CLI_MAX_URIS);
    try(grafcli_inner, &c);
    u8csbFree(c.flags);
    uribFree(c.uris);
    PATHu8bFree(c.repo);
    done;
}

MAIN(grafcli);
