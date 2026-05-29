#ifndef BEAGLE_DISPATCH_H
#define BEAGLE_DISPATCH_H

//  Declarative per-verb dispatch (DISPATCH.plan.md).
//
//  Each `be` verb is a static array of `be_action` rows.  Rows gate
//  on the **aggregate** URI-slot pattern (`URIPattern` OR-folded
//  across all input URIs).  The executor walks the table once,
//  firing each row whose gate matches; rows are inherently fire-
//  once.  Contiguous `parallel=YES` rows spawn together and reap
//  together (planned optimisation; currently serialised — see
//  DISPATCH.c).
//
//  Action functions live in `DISPATCH.c`.  See `DISPATCH.plan.md`
//  for the per-verb plans and the migration order.

#include "abc/01.h"
#include "abc/OK.h"
#include "abc/URI.h"
#include "dog/CLI.h"

//  Clean short-circuit sentinel — an action returns `BESTOP` to tell
//  the executor "this verb is done; exit OK".  Distinct from the
//  *NONE class (low byte 0xCE) which the executor also swallows;
//  *NONE means "this action had nothing to do, keep going", while
//  BESTOP means "stop the whole table".  Encoding chosen so the low
//  byte (0x19) doesn't collide with the *NONE class.
con ok64 BESTOP = 0x2ce71d619;

typedef struct be_action be_action;

//  Action body.  Receives the parsed cli; iterates `c->uris`
//  internally if it needs URI-specific work.  Returns:
//    OK            — continue with the next action.
//    *NONE class   — continue (action had nothing to do).
//    BESTOP        — short-circuit clean, executor returns OK.
//    other non-OK  — executor returns that code as the verb's exit.
typedef ok64 (*be_action_fn)(cli *c);

struct be_action {
    u8           require_mask;   // (agg_pat & require) == require
    u8           exclude_mask;   // (agg_pat & exclude) == 0
    b8           parallel;       // batch with adjacent parallel rows
    be_action_fn fn;
};

//  Sentinel: a `fn == NULL` row terminates a plan array.
#define BE_ACTION_END  { 0, 0, NO, NULL }

//  Executor.  Aggregates `URIPattern(c->uris[*])` and walks `plan`
//  once, firing each row whose gate matches.
ok64 BEExecute(cli *c, be_action const *plan);

//  --- Per-verb plans -------------------------------------------------

extern be_action const BE_PLAN_HEAD[];
extern be_action const BE_PLAN_GET[];
extern be_action const BE_PLAN_POST[];
extern be_action const BE_PLAN_PATCH[];
extern be_action const BE_PLAN_PUT[];
extern be_action const BE_PLAN_DELETE[];

//  --- Action library -------------------------------------------------
//
//  Reusable per-verb building blocks.  Real bodies land as each
//  verb migrates (see DISPATCH.plan.md §"Migration order").  Names
//  mirror the table.

//  URI rewriters (no spawn).
ok64 BEActPromoteRef    (cli *c);  // bareword → query on each URI
ok64 BEActPathFormCheck (cli *c);  // POST refuses path-form URIs
ok64 BEActBootstrap     (cli *c);  // be_ensure_project_repo
ok64 BEActWorktreeAnchor(cli *c);  // BEGetWorktree (rewrites first URI)
ok64 BEActGetBaseline   (cli *c);  // capture pre-checkout wtlog tail
                                   //  for BEActSubsGet to diff against

//  Sniff-side spawns (worktree work).
ok64 BEActSniffGet      (cli *c);
ok64 BEActSniffPut      (cli *c);
ok64 BEActSniffDelete   (cli *c);
ok64 BEActSniffPost     (cli *c);
ok64 BEActSniffPatch    (cli *c);

//  POST-side post-pass: refresh spot+graf indexes against the wt's
//  current tip (a new commit just landed).  Self-gates on dry-run.
ok64 BEActReindex       (cli *c);

//  Single-file GET short-circuit: `be get file.c?ref` → BESTOP.
ok64 BEActSingleFileGet (cli *c);

//  Keeper-side spawns (object store + transport).
ok64 BEActKeeperGet     (cli *c);
ok64 BEActKeeperPush    (cli *c);
ok64 BEActKeeperDelete  (cli *c);

//  Post-fetch URI rewriter — collapses transport URIs to local
//  `?<40hex>` form so sub-dogs only see local references.  Runs
//  after BEActKeeperGet so the freshly-written REFS row is
//  resolvable.
ok64 BEActResolveRemote (cli *c);

//  Indexer spawns (parallel-capable, after a keeper-side write).
ok64 BEActSpotGet       (cli *c);
ok64 BEActGrafGet       (cli *c);
ok64 BEActGrafHead      (cli *c);

//  Submodule recursion.
ok64 BEActSubsHead      (cli *c);
ok64 BEActSubsGet       (cli *c);
ok64 BEActSubsPost      (cli *c);

//  --- Shared helpers (defined in BE.cli.c) ---------------------------

//  Spawn / reap a sibling tool without/with waiting.  BERun (defined
//  in BE.cli.c, declared in SUBS.h) is the spawn-and-wait variant.
ok64 BESpawn(u8csc tool, u8css argv, pid_t *out_pid);
ok64 BEReap (pid_t pid,  u8csc tool);

//  Build `<dog> <verb> [--at <uri>] [flags...] [URIs...]` into
//  `args` — the batched argv shape that preserves dog-side
//  atomicity (sniff put / sniff post see every URI in one
//  invocation).  Caller pre-allocates `args` with
//  `4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS` slots.
void BEBuildArgv(u8csb args, u8csc dog, u8csc verb, cli *c);

//  Bareword-as-ref promotion: shift a path-shaped bareword from
//  `u->path` into `u->query` for ref-expecting verbs (POST / PATCH /
//  GET).  Idempotent; returns YES if anything moved.
b8 BEPromoteRef(uri *u);

//  Ensure a `.be/<project>/` shard exists at cwd for local writers
//  (PUT / POST / PATCH / DELETE on a fresh dir).  No-op when one
//  already exists or when `u` carries a remote authority.
ok64 BEEnsureProjectRepo(uri *u);

//  HEAD's submodule recursion body (extracted from the old `BEHead`
//  wrapper).  Pre-order: enumerate one level of subs in
//  `<c->repo>/.gitmodules`, fork-self into each mounted entry with
//  `be head [flags] [URI]`, report declared-but-not-mounted ones.
//  Best-effort: per-sub failures don't abort the walk.  Returns the
//  worst exit code observed.
ok64 BEHeadSubs(cli *c);

#endif
