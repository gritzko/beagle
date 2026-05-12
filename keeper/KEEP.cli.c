//  keeper CLI — thin wrapper: parse, open, exec, close.
//
//  Under the new arrangement (dog/DOG.md §10a) keeper does not push
//  ingested objects to graf/spot — those dogs reindex themselves
//  when `be get URI` spawns them in parallel after keeper completes.
//
#include "KEEP.h"
#include "RECV.h"
#include "REFADV.h"
#include "UNPK.h"
#include "WIRE.h"

#include <string.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/PRO.h"
#include "dog/CLI.h"
#include "dog/DOG.h"

//  Drop-in for `git-upload-pack <repo-path>`: read pkt-lines on stdin,
//  emit refs advertisement + pack response on stdout.  Stateless across
//  requests (one process per ssh invocation, like vanilla git).
//
//  Repo path comes from argv (parsed into c->uris[0].data) — it is
//  *not* derived from cwd, so this verb works under any ssh ForceCommand
//  config.  Path is opened read-only since serving never mutates state.
static ok64 keeper_upload_pack(cli *c) {
    sane(c);
    if (c->nuris < 1) {
        return KEEPFAIL;
    }
    u8cs path = {c->uris[0].data[0], c->uris[0].data[1]};
    if (u8csEmpty(path)) return KEEPFAIL;

    home h = {};
    call(HOMEOpenAt, &h, path, NO);
    call(KEEPOpen, &h, NO);

    refadv adv = {};
    call(REFADVOpen, &adv, &KEEP);
    call(REFADVEmit, STDOUT_FILENO, &adv);
    ok64 wo = WIREServeUpload(STDIN_FILENO, STDOUT_FILENO, &KEEP, &adv);
    REFADVClose(&adv);
    KEEPClose();
    HOMEClose(&h);
    return wo;
}

//  Drop-in for `git-receive-pack <repo-path>`: read pkt-lines + pack on
//  stdin, emit refs advertisement + unpack/per-ref status on stdout.
//  Stateless across requests (one process per ssh invocation).  Repo
//  path comes from argv (parsed into c->uris[0].data); rw mode is
//  required because push writes packs + REFS.
static ok64 keeper_receive_pack(cli *c) {
    sane(c);
    if (c->nuris < 1) {
        return KEEPFAIL;
    }
    u8cs path = {c->uris[0].data[0], c->uris[0].data[1]};
    if (u8csEmpty(path)) return KEEPFAIL;

    home h = {};
    call(HOMEOpenAt, &h, path, YES);
    call(KEEPOpen, &h, YES);

    refadv adv = {};
    call(REFADVOpen, &adv, &KEEP);
    call(REFADVEmit, STDOUT_FILENO, &adv);
    ok64 ro = RECVServe(STDIN_FILENO, STDOUT_FILENO, &KEEP, &adv);
    REFADVClose(&adv);

    KEEPClose();
    HOMEClose(&h);
    return ro;
}

static ok64 keepercli_inner(cli *c) {
    sane(c);
    call(FILEInit);
    call(CLIParse, c, KEEP_CLI_VERBS, KEEP_CLI_VAL_FLAGS);

    //  upload-pack short-circuits the standard cwd-derived HOME/KEEP
    //  open: it opens the repo named in argv (the ssh contract), runs
    //  the wire negotiator on stdin/stdout, and exits.  Falls through
    //  to the generic dispatch on argless invocation.
    a_cstr(v_upload_pack, "upload-pack");
    if ($eq(c->verb, v_upload_pack)) {
        return keeper_upload_pack(c);
    }
    a_cstr(v_receive_pack, "receive-pack");
    if ($eq(c->verb, v_receive_pack)) {
        return keeper_receive_pack(c);
    }

    a_cstr(v_status, "status");
    a_cstr(v_refs,   "refs");
    a_cstr(v_verify, "verify");
    b8 ro = $eq(c->verb, v_status) || $eq(c->verb, v_refs)
         || $eq(c->verb, v_verify);
    //  Verb-less projector dispatch (`keeper tree:?...`, `commit:`,
    //  `blob:`) is also read-only — no pack ingest, no ref writes.
    if (!ro && $empty(c->verb) && c->nuris > 0) {
        char const *dog = DOGProjectorDog(c->uris[0].scheme);
        if (dog != NULL && strcmp(dog, "keeper") == 0) ro = YES;
    }
    b8 rw = !ro;

    //  Prefer `--at` from be; fall back to cwd-walk via c->repo.
    home h = {};
    uri at = {};
    CLIAtURI(&at, c);
    if (u8csEmpty(at.path) && u8bHasData(c->repo))
        u8csMv(at.path, $path(c->repo));
    call(HOMEOpen, &h, &at, rw);

    //  Branch-aware open: when the first verb URI carries a `?branch`
    //  query, that's the explicit target the caller wants keeper
    //  pointed at — load it as the leaf (PAST = trunk → branch's
    //  ancestors, DATA = branch shard).  Falls back to the `--at`
    //  URI's cur_branch (wt-context, set by `be` from the wtlog) and
    //  finally trunk.  URI query wins over `--at` so wire receives
    //  (`keeper get file://...?fix2`) land in the target branch's
    //  shard even when the wt is on a different cur.
    //  Empty-but-valid slice: both ends point at the same byte so
    //  the `$ok(branch)` sanity check in KEEPOpenBranch holds.
    static u8c const _zero = 0;
    u8cs branch = {&_zero, &_zero};
    if (c->nuris > 0 && !u8csEmpty(c->uris[0].query))
        u8csMv(branch, c->uris[0].query);
    else if (u8bHasData(h.cur_branch))
        u8csMv(branch, u8bDataC(h.cur_branch));
    call(KEEPOpenBranch, &h, branch, rw);

    ok64 ret = KEEPExec(&KEEP, c);

    KEEPClose();
    HOMEClose(&h);
    return ret;
}

ok64 keepercli() {
    sane(1);
    cli c = {};
    call(PATHu8bAlloc, c.repo);
    try(keepercli_inner, &c);
    PATHu8bFree(c.repo);
    done;
}

MAIN(keepercli);
