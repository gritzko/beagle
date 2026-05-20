//  sniff CLI — thin wrapper: parse, open, exec, close.
//
//  Sniff has no shard index of its own (DOG.md §"Indexing"); it
//  reads worktree state and the `.be/wtlog` ULOG, and writes commits
//  through keeper.  Reindexing of graf/spot is no longer driven
//  from here — under the new arrangement (DOG.md §10a) `be` spawns
//  spot/graf alongside keeper, and each dog refreshes its own
//  index from keeper's read APIs.
//
#include "SNIFF.h"

#include <unistd.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PRO.h"
#include "abc/URI.h"
#include "dog/CLI.h"
#include "dog/DOG.h"
#include "graf/GRAF.h"
#include "keeper/KEEP.h"
#include "AT.h"

//  Pure-work body: opens already done by the wrapper.  Runs the verb
//  via SNIFFExec; on rw verbs that committed, walks the new tip into
//  graf's DAG so subsequent LCAs see it.
//
//  ABC.md §"Resource lifecycle": worker functions receive resources,
//  never own them.  `h`, the SNIFF singleton, and the graf singleton
//  are all opened/closed in sniffcli_inner (the enclosing frame); we
//  only read them here.
static ok64 sniffcli_body(cli *c, b8 rw, b8 needs_graf, ok64 go,
                          u8cs reporoot) {
    sane(c);
    ok64 ret = SNIFFExec(c);
    if (rw && needs_graf && ret == OK && (go == OK || go == GRAFOPEN)) {
        a_pad(u8, tail_buf, FILE_PATH_MAX_LEN + 128);
        if (SNIFFAtTailOf(reporoot, tail_buf) == OK) {
            uri tip = {};
            u8csMv(tip.data, u8bDataC(tail_buf));
            URILexer(&tip);
            (void)GRAFIndexFromTips(&tip);
        }
    }
    return ret;
}

//  Open/close frame: every successful open is paired with its close
//  on every exit path.  ABC.md §"Resource lifecycle" — singletons
//  are allocated at the top of the call chain; `call(...)` early-
//  returns past pending cleanup would leak, so the openers run via
//  `try()` and the closers fire unconditionally at the bottom.
static ok64 sniffcli_inner(cli *c) {
    sane(c);
    call(FILEInit);
    call(CLIParse, c, SNIFF_VERBS, SNIFF_VAL_FLAGS);

    char cwd[1024];
    u8cs reporoot = {};
    if (u8bHasData(c->repo)) {
        u8csMv(reporoot, $path(c->repo));
    } else {
        if (!getcwd(cwd, sizeof(cwd))) fail(SNIFFFAIL);
        a_cstr(cwds, cwd);
        (void)PATHu8bFeed(c->repo, cwds);
        u8csMv(reporoot, $path(c->repo));
    }

    // Help and stop don't need an open state.
    a_cstr(v_help, "help");
    a_cstr(v_stop, "stop");
    b8 need_state = !u8csEq(c->verb, v_help) && !u8csEq(c->verb, v_stop)
                 && !CLIHas(c, "-h") && !CLIHas(c, "--help");

    if (!need_state) return SNIFFExec(c);

    // rw for anything that mutates the ULOG at `<wt>/.be/wtlog` or the
    // store.  View projectors (verbless `sniff <proj>:<URI>`) are
    // always RO per VERBS.md §"View projectors are pure".  Bare
    // `sniff post` (no -m, no `?label`) is a dry-run change-set
    // print — also RO; otherwise FILEBook's page-align grows .be/wtlog
    // and ULOGClose can't trim under a non-dirty handle.
    a_cstr(v_status, "status");
    a_cstr(v_list,   "list");
    a_cstr(v_post,   "post");
    a_cstr(v_commit, "commit");
    b8 is_projector = u8csEmpty(c->verb) && c->nuris > 0 &&
                      DOGIsProjector(c->uris[0].scheme);
    b8 ro = u8csEq(c->verb, v_status) || u8csEq(c->verb, v_list) || is_projector;
    b8 rw = !ro;

    //  Prefer the explicit `--at <root>?<branch>#<sha>` flag forwarded
    //  by `be`; falls back to the legacy reporoot path (cwd-walk via
    //  HOMEOpen when reporoot is empty).
    home h = {};
    uri at = {};
    CLIAtURI(&at, c);
    if (u8csEmpty(at.path) && u8csOK(reporoot) && !u8csEmpty(reporoot))
        u8csMv(at.path, reporoot);

    //  HOMEOpen failure self-cleans via its own try/nedo wrapper, so
    //  we can `call(...)` it safely.  From here on we own `h` and
    //  every exit path must HOMEClose.
    call(HOMEOpen, &h, &at, rw);

    //  SNIFFOpen via try() so its failure routes through the cleanup
    //  below instead of skipping HOMEClose.
    try(SNIFFOpen, &h, rw);   // opens keeper singleton too
    ok64 so = __;
    if (so != OK) { HOMEClose(&h); return so; }

    //  SNIFFOpen zerops the singleton — set verb-side flags AFTER.
    SNIFF.nosub = CLIHas(c, "--nosub") ? YES : NO;
    SNIFF.force = CLIHas(c, "--force") ? YES : NO;
    SNIFF.prune = CLIHas(c, "--prune") ? YES : NO;
    SNIFF.quiet = (CLIHas(c, "-q") || CLIHas(c, "--quiet")) ? YES : NO;

    //  graf needs to be open whenever sniff may consult the DAG
    //  (POST ff-check, PATCH 3-way LCA, PUT branch creation rebase,
    //  DELETE branch ancestor checks).  Do NOT open it for
    //  `get`/`checkout`/`sub-mount` — those don't read graf, and they
    //  fork a parallel `graf get` child (or the recursive `be get`
    //  inside SubMount does) whose lock would race with the sniff-rw
    //  lock if we opened it here (long flock waits on big-repo
    //  clones; sub-mount deadlocks the depth-3 case outright).
    a_cstr(v_get,      "get");
    a_cstr(v_patch,    "patch");
    a_cstr(v_checkout, "checkout");
    a_cstr(v_submount, "sub-mount");
    b8 needs_graf = !u8csEq(c->verb, v_get) && !u8csEq(c->verb, v_checkout)
                 && !u8csEq(c->verb, v_submount)
                 && (rw || u8csEq(c->verb, v_post) || u8csEq(c->verb, v_commit)
                        || u8csEq(c->verb, v_patch));
    //  Branch-aware graf open mirrors keeper: per-branch shard
    //  dirs (`.be/<branch>/<seqno>.graf.idx`).  Cross-branch DAG
    //  walks switch via `SNIFFMaybeSwitchGraf` in each verb.
    a_pad(u8, gbr_buf, 256);
    if (needs_graf) {
        ron60 bts = 0, bverb = 0;
        uri bu = {};
        if (SNIFFAtCurTip(&bts, &bverb, &bu) == OK)
            u8bFeed(gbr_buf, bu.query);
    }
    a_dup(u8c, gbranch, u8bData(gbr_buf));
    ok64 go = needs_graf ? GRAFOpenBranch(&h, gbranch, rw) : NONE;

    //  Body via try() so its non-OK exit routes through the cleanup
    //  below instead of skipping the closes.
    try(sniffcli_body, c, rw, needs_graf, go, reporoot);
    ok64 ret = __;

    if (go == OK) GRAFClose();
    SNIFFClose();
    HOMEClose(&h);
    return ret;
}

ok64 sniffcli() {
    sane(1);
    cli c = {};
    call(PATHu8bAlloc, c.repo);
    try(sniffcli_inner, &c);
    ok64 ret = __;
    //  `-q` / `--quiet` swallows POSTNONE (a no-op signal, not a
    //  real error) so callers like `be post` recursion don't see
    //  "Error: POSTNONE" for every sibling shard with no changes.
    if (ret == POSTNONE &&
        (CLIHas(&c, "-q") || CLIHas(&c, "--quiet")))
        ret = OK;
    PATHu8bFree(c.repo);
    return ret;
}

MAIN(sniffcli);
