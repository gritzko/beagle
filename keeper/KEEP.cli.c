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
#include "abc/HEX.h"
#include "abc/PRO.h"
#include "dog/CLI.h"
#include "dog/DOG.h"

//  Drop-in for `git-upload-pack <repo-path>`: read pkt-lines on stdin,
//  emit refs advertisement + pack response on stdout.  Stateless across
//  requests (one process per ssh invocation, like vanilla git).
//
//  Repo path comes from argv (parsed into uribAtP(c->uris, 0)->data) — it is
//  *not* derived from cwd, so this verb works under any ssh ForceCommand
//  config.  Path is opened read-only since serving never mutates state.
//  Split the served argv into a raw store path + optional `?/proj`
//  selector, WITHOUT URI-parsing: the peer may send a `//abs` path
//  (`//home/…`) whose first segment the URI lexer mis-reads as an
//  authority (dropping a leading dir → FILEACCES).  Everything before
//  the first `?` is the store path verbatim; the rest is the project
//  query the keeper-peer client appended (empty ⇒ row-0 default).
static void keeper_served_at(uri *at, uri *g) {
    u8cs data = {g->data[0], g->data[1]};
    u8c const *q = NULL;
    $for(u8c, p, data) if (*p == '?') { q = p; break; }
    at->path[0] = data[0];
    at->path[1] = q ? q : data[1];
    if (q && q + 1 < data[1]) {
        at->query[0] = q + 1;
        at->query[1] = data[1];
    }
}

//  Worker: keeper is already open via HOMEOpen; this carries the
//  KEEP/REFADV opens + wire serve.  Any `call` failure returns to the
//  wrapper's `try`, which runs the (idempotent / null-safe) closers —
//  so a half-open never leaks `h`'s buffers.
static ok64 keeper_upload_pack_inner(home *h) {
    sane(h);
    //  Refuse a non-existent store up front (git-parity): a RO open
    //  tolerates an absent shard (KEEPOpenBranch reads zero refs), so
    //  without this gate `upload-pack <bad-repo>` would advertise an
    //  empty `0000` then block on read(0) — a serve that can never ship
    //  anything.  HOMEProjectExists gates on the store DIR existing (not
    //  refs being non-empty): a shard that holds objects but advertises
    //  zero refs is still served (want-by-hash pin fetch — see
    //  WIRE_CLIENT fetch_by_pin).  Fail fast before any advertisement.
    a_dup(u8c, proj, u8bDataC(h->project));
    call(HOMEProjectExists, h, proj);
    call(KEEPOpen, h, NO);

    a_refadv(adv);
    call(REFADVOpen, &adv);
    call(REFADVEmit, STDOUT_FILENO, &adv);
    ok64 wo = WIREServeUpload(STDIN_FILENO, STDOUT_FILENO, &adv);
    REFADVClose(&adv);
    return wo;
}

static ok64 keeper_upload_pack(cli *c) {
    sane(c);
    if (uribDataLen(c->uris) < 1) {
        return KEEPFAIL;
    }
    uri *g = uribAtP(c->uris, 0);
    u8cs path = {g->data[0], g->data[1]};
    if (u8csEmpty(path)) return KEEPFAIL;

    //  Serve the requested project: a `?/proj` selector forwarded to
    //  HOMEOpen routes through that shard (home_open_inner step 5).
    //  Absent ⇒ HOMEOpen falls back to the store's row-0 default.
    home h = {};
    uri at = {};
    keeper_served_at(&at, g);
    call(HOMEOpen, &h, &at, NO);
    try(keeper_upload_pack_inner, &h);
    KEEPClose();
    HOMEClose(&h);
    done;
}

//  Drop-in for `git-receive-pack <repo-path>`: read pkt-lines + pack on
//  stdin, emit refs advertisement + unpack/per-ref status on stdout.
//  Stateless across requests (one process per ssh invocation).  Repo
//  path comes from argv (parsed into uribAtP(c->uris, 0)->data); rw mode is
//  required because push writes packs + REFS.
static ok64 keeper_receive_pack_inner(home *h) {
    sane(h);
    call(KEEPOpen, h, YES);

    a_refadv(adv);
    call(REFADVOpen, &adv);
    call(REFADVEmit, STDOUT_FILENO, &adv);
    ok64 ro = RECVServe(STDIN_FILENO, STDOUT_FILENO, &adv);
    REFADVClose(&adv);
    return ro;
}

static ok64 keeper_receive_pack(cli *c) {
    sane(c);
    if (uribDataLen(c->uris) < 1) {
        return KEEPFAIL;
    }
    uri *g = uribAtP(c->uris, 0);
    u8cs path = {g->data[0], g->data[1]};
    if (u8csEmpty(path)) return KEEPFAIL;

    //  Serve the requested project (see keeper_upload_pack); absent ⇒
    //  row-0 default.
    home h = {};
    uri at = {};
    keeper_served_at(&at, g);
    call(HOMEOpen, &h, &at, YES);
    try(keeper_receive_pack_inner, &h);
    KEEPClose();
    HOMEClose(&h);
    //  Lock is released — safe to spawn `be get ?` in the colocated
    //  primary wt (if any).  See RECV.h §RECVAdvanceColocatedWt.
    RECVAdvanceColocatedWt();
    done;
}

static ok64 keepercli_inner(cli *c) {
    sane(c);
    call(FILEInit);
    call(CLIParse, c, KEEP_CLI_VERBS, KEEP_CLI_VAL_FLAGS);
    CLISetHUNKMode(c);

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
    if (!ro && $empty(c->verb) && uribDataLen(c->uris) > 0) {
        char const *dog = DOGProjectorDog(uribAtP(c->uris, 0)->scheme);
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

    //  Branch-aware open: pick the leaf shard the verb will operate on.
    //
    //  Two distinct concepts share the URI's query slot:
    //    1. LOCAL branch switch (`be get ?feat`) — query names the
    //       branch.  No authority on the URI.
    //    2. REMOTE ref fetch (`be get ssh://host/path?master`) — query
    //       names the ref the peer should advertise/send.  The local
    //       branch (where the fetched objects land) is the wt's
    //       cur_branch, NOT the remote ref name.  In submodule
    //       contexts that branch is parked in `h.cur_branch` by
    //       HOMEOpen from the row-0 anchor's `/.be/<basename>/`.
    //
    //  Sha-detached form (`?<40-hex>`) is also NOT a branch.
    //
    //  Rule: query is the leaf only when there is no authority (purely
    //  local checkout) and the query isn't a 40-hex sha.  Otherwise
    //  fall back to cur_branch.
    //
    //  Empty-but-valid slice: both ends point at the same byte so
    //  the `$ok(branch)` sanity check in KEEPOpenBranch holds.
    static u8c const _zero = 0;
    u8cs branch = {&_zero, &_zero};
    b8 has_query    = (uribDataLen(c->uris) > 0 && !u8csEmpty(uribAtP(c->uris, 0)->query));
    b8 query_is_sha = (has_query && DOGIsFullSha(uribAtP(c->uris, 0)->query));
    b8 has_authority = (uribDataLen(c->uris) > 0 && !u8csEmpty(uribAtP(c->uris, 0)->authority));

    //  Remote fetch into a project-less store: derive the project
    //  (Title) from the SOURCE URL — Store.mkd "shard named by Title =
    //  URL basename" — so fetched objects land sharded in
    //  `.be/<title>/`, not flat.  `be get` sets the project via --at;
    //  direct `keeper get` derives it here.  Local checkouts (no
    //  authority) keep the store's own project (HOMEOpen single-shard
    //  scan).
    if (rw && has_authority && u8bEmpty(h.project))
        DOGTitleFromUri(uribAtP(c->uris, 0), h.project);
    //  Remote-vs-local branch resolution:
    //    Remote (`scheme://host…?ref`) — the query is the REMOTE ref
    //      to fetch.  The local branch (where fetched objects land)
    //      is the wt's cur_branch (parked by HOMEOpen from the row-0
    //      anchor for sub-shard contexts).  Fresh-clone fallback is
    //      TRUNK (empty branch) — a first wire fetch lands its whole
    //      reachable closure in the project root pack so subsequent
    //      sibling-ref fetches can resolve objects via the standard
    //      child→parent→root shard walk.  Landing the initial pack in
    //      the requested ref's leaf isolates it from later siblings
    //      (e.g. `?tags/v1` then `?master` would never find master's
    //      objects in tags/v1's sibling shard).
    //    Local (`?ref`, no authority) — query IS the branch name
    //      (switch op); falls back to cur_branch when query is
    //      empty or a sha (detached pin, not a branch).
    if (has_authority) {
        if (u8bHasData(h.cur_branch))
            u8csMv(branch, u8bDataC(h.cur_branch));
        //  else: fresh clone — leave `branch` empty (trunk).
    } else {
        if (has_query && !query_is_sha)
            u8csMv(branch, uribAtP(c->uris, 0)->query);
        else if (u8bHasData(h.cur_branch))
            u8csMv(branch, u8bDataC(h.cur_branch));
    }
    //  Absolute query (`?/<project>/<branch>`) carries a project
    //  prefix that's local-side state (already consumed by
    //  home_open_inner / be_ensure_project_repo).  Strip it so
    //  KEEPOpenBranch normalises the BRANCH portion only — otherwise
    //  `?/U` would mint a phantom local branch named `U` under
    //  project `U` (.be/U/U/).  Per https://replicated.wiki/html/wiki/URI.html §"Ref resolution".
    DOGQueryStripProject(branch);
    call(KEEPOpenBranch, &h, branch, rw);

    ok64 ret = KEEPExec(c);

    KEEPClose();
    HOMEClose(&h);
    return ret;
}

ok64 keepercli() {
    sane(1);
    cli c = {};
    call(PATHu8bAlloc, c.repo);
    call(u8csbAlloc, c.flags, CLI_MAX_FLAGS * 2);
    call(uribAlloc,  c.uris,  CLI_MAX_URIS);
    try(keepercli_inner, &c);
    u8csbFree(c.flags);
    uribFree(c.uris);
    PATHu8bFree(c.repo);
    done;
}

MAIN(keepercli);
