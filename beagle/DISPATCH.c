//  Per-verb dispatch executor + action library (DISPATCH.plan.md).
//
//  The executor aggregates `URIPattern` across `c->uris` and walks
//  the action table once, firing each row whose gate matches.
//  Action bodies receive only `cli *c` and pass `c->uris` en bloc
//  to the dog spawn — preserving batch atomicity (sniff put, sniff
//  post, keeper post see every URI in one invocation).

#include "DISPATCH.h"
#include "SUBS.h"          // BERun

#include "abc/PRO.h"
#include "abc/PATH.h"
#include "abc/URI.h"
#include "dog/DOG.h"       // DOGDebangSlice / DOGDebangFeed / DOG_BANG_*
#include "dog/HOME.h"
#include "keeper/REFS.h"
#include "graf/GRAF.h"     // GRAFNONE (HEAD-002 leg-flavour mapping)

//  --- Executor ------------------------------------------------------

ok64 BEExecute(cli *c, be_action const *plan) {
    sane(c && plan);

    //  Aggregate URI-slot pattern across every input URI.
    u8 pat = 0;
    for (u32 i = 0; i < uribDataLen(c->uris); i++) {
        pat |= URIPattern(uribAtP(c->uris, i));
    }

    //  *NONE-class codes within a row mean "this action had nothing
    //  to do — continue with the next row".  But the *latest* NONE
    //  seen must still surface as the verb's exit code: tests like
    //  `be head '#nomatch'` rely on GRAFNONE propagating out so the
    //  shell can detect "no match".  Real errors (non-NONE non-OK)
    //  abort immediately.
    ok64 last_none = OK;
    for (be_action const *a = plan; a->fn != NULL; a++) {
        if ((pat & a->require_mask) != a->require_mask) continue;
        if ((pat & a->exclude_mask) != 0)               continue;

        //  Parallel batching is a planned optimisation: contiguous
        //  `parallel=YES` rows should fan out via BESpawn and reap
        //  together.  For now every row runs serially regardless of
        //  the flag — correctness first, perf later.
        ok64 rc = a->fn(c);
        if (rc == BESTOP)             return OK;
        if (rc == OK)                 continue;
        if (ok64is(rc, NONE))         { last_none = rc; continue; }
        return rc;
    }
    return last_none;
}

//  --- Spawn helper --------------------------------------------------
//
//  Compose `<dog> <verb> [--at] [flags] [URIs...]` and run it
//  synchronously via BERun.  Batched argv shape — every URI in
//  `c->uris` is forwarded together so the dog can apply slot
//  composition (POST's #frag + ?ref + //remote) or atomic batch
//  semantics (sniff put's auto-pair across multi-arg lists).

static ok64 be_spawn_all(char const *dog_cstr, char const *verb_cstr,
                         cli *c) {
    sane(c);
    a_cstr(dog_s,  dog_cstr);
    a_cstr(verb_s, verb_cstr);
    a_dup(u8c, dog_d,  dog_s);
    a_dup(u8c, verb_d, verb_s);
    a_pad(u8cs, args, 4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS);
    BEBuildArgv(args, dog_d, verb_d, c);
    a_dup(u8cs, argv, u8csbData(args));
    return BERun(dog_d, argv, NO);
}

//  --- Action library: URI rewriters --------------------------------

ok64 BEActPromoteRef(cli *c) {
    sane(c);
    for (u32 i = 0; i < uribDataLen(c->uris); i++) {
        (void)BEPromoteRef(uribAtP(c->uris, i));
    }
    done;
}

//  After a wire fetch (BEActKeeperGet) has written the remote-
//  tracking ref row to `<store>/<project>/refs`, transport URIs
//  (`scheme://host?ref`) carry no more useful information for the
//  downstream sub-dogs (graf, sniff) — they only need the local
//  sha the fetch landed.  Sub-dogs that re-resolve the bare query
//  alone (`?master`) hit the LOCAL ref of that name, which on a
//  non-FF fetch points at cur, NOT at the freshly-fetched
//  theirs.tip.  Rewriting `scheme://host?ref` to a local `?<sha>`
//  shape eliminates this footgun and centralises authority
//  handling in BE.
//
//  Rewrite shape (caller-bytes scratch): clear scheme, authority,
//  host, path; replace query with the 40-hex sha; data is rebuilt
//  as the literal `?<40hex>`.  Sub-dogs see PATCH_SHAPE_SQUASH /
//  CHERRY / MERGE / REBASE1 classification unchanged (the leading
//  `?` and any user-supplied `#frag` still mark the same shape).
//
//  Skipped for URIs that lack an authority or whose query already
//  resolves through a local row (no-op fast path).
ok64 BEActResolveRemote(cli *c) {
    sane(c);
    if (uribDataLen(c->uris) == 0) done;

    static u8 _resolved_scratch[CLI_MAX_URIS * 64];
    u8b scratch = {_resolved_scratch,
                   _resolved_scratch,
                   _resolved_scratch,
                   _resolved_scratch + sizeof(_resolved_scratch)};

    a_path(keepdir);
    {
        home h = {};
        uri at = {};
        CLIAtURI(&at, c);
        if (u8csEmpty(at.path) && u8bHasData(c->repo))
            u8csMv(at.path, $path(c->repo));
        if (HOMEOpen(&h, &at, NO) != OK) { HOMEClose(&h); done; }
        if (HOMEBranchDir(&h, keepdir, NULL) != OK) {
            HOMEClose(&h); done;
        }
        HOMEClose(&h);
    }

    for (u32 i = 0; i < uribDataLen(c->uris); i++) {
        uri *u = uribAtP(c->uris, i);
        if (u8csEmpty(u->authority)) continue;
        if (u8csEmpty(u->data))      continue;

        //  URI-002: a trailing `!` on the query is the PATCH whole-branch
        //  scope modifier (DOG_BANG_QUERY), not part of the ref.  Read it
        //  via the uniform debanger off a local copy of the query, then
        //  re-add it to the rewritten local `?<sha>!` so the scope
        //  survives the transport→local rewrite all the way to sniff
        //  PATCH.  (The wire fetch already sheds it in keeper's
        //  wcli_be_to_wire.)
        u8 bang = 0;
        a_dup(u8c, qbang, u->query);
        if (DOGDebangSlice(qbang)) bang |= DOG_BANG_QUERY;
        b8 whole_scope = (bang & DOG_BANG_QUERY) != 0;

        //  Resolve against a `!`-shed view of the URI: the modifier sits
        //  at the very tail (query is the last component on a fragment-
        //  less patch URI), so a single tail-shed yields the bare ref the
        //  local tracking ref was fetched under.
        a_dup(u8c, resolve_in, u->data);
        if (whole_scope) (void)DOGDebangSlice(resolve_in);

        a_pad(u8, arena, 1024);
        uri resolved = {};
        ok64 ro = REFSResolve(&resolved, arena, $path(keepdir), resolve_in);
        if (ro != OK || u8csLen(resolved.query) != 40) continue;

        u8cs frag_save = {u->fragment[0], u->fragment[1]};

        //  Compose `?<sha>[!]` or `?<sha>#<frag>` into the persistent
        //  scratch buffer; the result outlives this BE plan frame
        //  so BEBuildArgv can forward `u->data` to the sub-dog argv.
        //  Re-attach `#` whenever the fragment is PRESENT — including a
        //  present-but-empty one — not merely when it's non-empty: an
        //  empty `#` is the rebase-one marker (patch_shape's `has_f`).
        //  Testing `u8csEmpty` here dropped it and degraded
        //  `?<branch>#` (rebase-one) to `?<branch>` (squash), so the
        //  next POST couldn't reuse the replayed commit's message.
        b8 has_frag = (frag_save[0] != NULL);
        u8c *uri_before = u8bIdleHead(scratch);
        if (u8bFeed1(scratch, '?') != OK) continue;
        if (u8bFeed (scratch, resolved.query) != OK) continue;
        //  URI-002: re-emit the query-bang via the uniform feeder so the
        //  rewritten `?<sha>!` carries the whole-branch scope downstream.
        DOGDebangFeed(scratch, bang, DOG_BANG_QUERY);
        if (has_frag) {
            if (u8bFeed1(scratch, '#') != OK) continue;
            if (!u8csEmpty(frag_save) &&
                u8bFeed(scratch, frag_save) != OK) continue;
        }
        u8c *uri_after = u8bIdleHead(scratch);
        u8cs full_uri = {uri_before, uri_after};

        //  URILexer CONSUMES u->data — save a copy of the bytes
        //  pointer pair separately so we can restore after parsing.
        zerop(u);
        u8csMv(u->data, full_uri);
        if (URILexer(u) != OK) continue;
        //  Restore u->data to the full URI span (URILexer left it
        //  pointing past the consumed bytes).
        u8csMv(u->data, full_uri);
    }
    done;
}

//  BEActPathFormCheck lives in BE.cli.c — close to its
//  `be_post_is_path_form` helper.

ok64 BEActBootstrap(cli *c) {
    sane(c);
    //  The legacy BEPut/BEDelete/BEPostLocal each took the first URI
    //  as the project-name probe (the only URI that could carry
    //  `?/<proj>/...`).  Same shape here.
    uri *u0 = (uribDataLen(c->uris) > 0) ? uribAtP(c->uris, 0) : NULL;
    return BEEnsureProjectRepo(u0);
}

//  BEActWorktreeAnchor, BEActGetBaseline, BEActSingleFileGet,
//  BEActSubsGet, BEActSubsHead live in BE.cli.c — they need access
//  to BE.cli.c-internal helpers and/or static cross-action state.

//  --- Action library: sniff-side spawns ----------------------------

ok64 BEActSniffPut    (cli *c) { return be_spawn_all("sniff", "put",    c); }
ok64 BEActSniffDelete (cli *c) { return be_spawn_all("sniff", "delete", c); }

ok64 BEActSniffGet    (cli *c) { return be_spawn_all("sniff", "get",   c); }
ok64 BEActSniffPost   (cli *c) { return be_spawn_all("sniff", "post",  c); }
ok64 BEActSniffPatch  (cli *c) { return be_spawn_all("sniff", "patch", c); }

//  BEActSingleFileGet, BEActWorktreeAnchor, BEActGetBaseline, and the
//  three subs-recursion actions live in BE.cli.c — they need access
//  to static BE.cli.c state (the rewrite buffer, the captured baseline
//  wtlog tail) or to internal helpers (BEGetWorktree, BEGet*Subs).

//  --- Action library: keeper-side spawns ---------------------------

ok64 BEActKeeperGet    (cli *c) { return be_spawn_all("keeper", "get",    c); }
ok64 BEActKeeperPush   (cli *c) { return be_spawn_all("keeper", "post",   c); }

//  PUT-to-remote: same wire path as POST-push (BEActKeeperPush) but
//  with a `--force` flag so keeper-side skips its FF check.  PUT is
//  unconstrained per https://replicated.wiki/html/wiki/PUT.html §PUT Design invariant 9.
ok64 BEActKeeperPushForce(cli *c) {
    sane(c);
    a_cstr(dog_s,  "keeper");
    a_cstr(verb_s, "post");
    a_cstr(force_s, "--force");
    a_dup(u8c, dog_d,  dog_s);
    a_dup(u8c, verb_d, verb_s);
    a_dup(u8c, force_d, force_s);
    a_pad(u8cs, args, 5 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS);
    BEBuildArgv(args, dog_d, verb_d, c);
    //  Append --force to the spawn argv (BEBuildArgv already wrote
    //  dog/verb/flags/uris in order; we add one more flag at the
    //  end — keeper's CLIParse handles flags positionally with the
    //  URIs so the trailing position is fine).
    u8csbFeed1(args, force_d);
    a_dup(u8cs, argv, u8csbData(args));
    return BERun(dog_d, argv, NO);
}
ok64 BEActKeeperDelete (cli *c) { return be_spawn_all("keeper", "delete", c); }

//  --- Action library: indexer spawns -------------------------------

ok64 BEActSpotGet  (cli *c) { return be_spawn_all("spot", "get",  c); }
ok64 BEActGrafGet  (cli *c) { return be_spawn_all("graf", "get",  c); }

//  HEAD-002 (facet 2): the graf-head leg's exit byte (POSIX-truncated)
//  can only carry the generic NONE low byte (0xCE), so `BERun` collapses
//  any *NONE — including graf's GRAFNONE "cannot resolve target" / "no
//  commit message matches" — to the generic `NONE`.  `BEExecute` then
//  surfaces that as a trailing `Error: NONE`, which CONTRADICTS the
//  `Error: GRAFNONE` graf's own MAIN already printed.  This leg knows it
//  spawned `graf head`, so re-flavour the NONE-class result back to
//  GRAFNONE: the be-level `Error:` now agrees with graf's, the exit code
//  is unchanged (both share the 0xCE low byte → exit 206, still
//  non-zero), and a real graf failure can never read as a benign
//  no-op.  A genuine GRAFFAIL/BEDOGEXIT leg (non-NONE) propagates
//  verbatim — never swallowed, never exit 0.
ok64 BEActGrafHead (cli *c) {
    ok64 r = be_spawn_all("graf", "head", c);
    if (ok64is(r, NONE)) return GRAFNONE;
    return r;
}

//  --- Action library: submodule recursion --------------------------

ok64 BEActSubsHead (cli *c) {
    //  Body lives in BE.cli.c so the recursion logic (transport-mode
    //  URL swap, lazy stderr markers, etc.) sits next to the
    //  recurse_cb closure it drives.
    return BEHeadSubs(c);
}

//  BEActSubsGet / BEActSubsPost / BEActReindex: see BE.cli.c.

//  --- Per-verb plans ------------------------------------------------
//
//  Rows whose actions are still stubs (GET/POST/PATCH bodies above)
//  fill in during their respective migration stages.

be_action const BE_PLAN_HEAD[] = {
    //  Transport URI (scheme + authority) → fetch remote refs via
    //  keeper, then re-walk into spot/graf indexes.  Per [URI]/[HEAD]
    //  the SCHEME alone flips cached vs wire: `//origin` (authority,
    //  NO scheme) reads the cache, `ssh://origin` opens the wire.  So
    //  the keeper/reindex wire arms require URI_SCHEME|URI_AUTHORITY,
    //  mirroring BE_PLAN_GET's network gate (DIS-016); gating on
    //  authority alone fetched a cached `//origin` over the wire.
    //  A bare SCHEME-ONLY transport URI (`ssh:` / `file:` — scheme but
    //  no authority AND no path, GET-002 part 2) routes to keeper too:
    //  it completes from the recentmost same-scheme get/post row.
    //  Projectors are filtered out before any plan runs (BE.cli.c), so
    //  a URI_SCHEME bit here is always a transport scheme.  Excluding
    //  URI_PATH keeps a path-bearing local form (`file:<store>/.be/...`
    //  sibling-worktree wiring) on its own checkout pipeline.  GrafHead
    //  is the cached cur-vs-remote diff: it excludes only URI_SCHEME, so
    //  authority-WITHOUT-scheme (`//origin`) reads the cache here with
    //  no network, while a transport scheme suppresses the local diff
    //  in favour of the fetch arms above.
    { URI_SCHEME|URI_AUTHORITY, 0,            NO,  BEActKeeperGet },
    { URI_SCHEME,    URI_AUTHORITY|URI_PATH,  NO,  BEActKeeperGet },
    { URI_SCHEME|URI_AUTHORITY, 0,            YES, BEActSpotGet   },
    { URI_SCHEME,    URI_AUTHORITY|URI_PATH,  YES, BEActSpotGet   },
    { URI_SCHEME|URI_AUTHORITY, 0,            YES, BEActGrafGet   },
    { URI_SCHEME,    URI_AUTHORITY|URI_PATH,  YES, BEActGrafGet   },
    //  HEAD-001: after the transport fetch+reindex above has landed
    //  the remote tip into the local cache, run the SAME cached
    //  cur-vs-remote diff the no-scheme `//origin` form runs below so
    //  `be head ssh://host?ref` prints the `ahead N, behind M` summary
    //  + differing-file rows instead of exiting 0 with empty stdout (a
    //  behind/diverged branch otherwise read identical to up-to-date,
    //  masking the divergence until a later POST hit WIRECLNFF).
    //  graf head's target resolver (REFSResolve, authority-substring)
    //  ignores the leading transport scheme, so it reads the freshly-
    //  fetched peer-tracking ref with no second wire trip — gating
    //  these rows to fire only AFTER the keeper/spot/graf-get rows.
    //  Mirror the wire arm's two gates (scheme+authority, scheme-only)
    //  so both transport shapes get the summary; the no-scheme cached
    //  row below stays exclusive (it excludes URI_SCHEME).
    { URI_SCHEME|URI_AUTHORITY, 0,            NO,  BEActGrafHead  },
    { URI_SCHEME,    URI_AUTHORITY|URI_PATH,  NO,  BEActGrafHead  },
    { 0,             URI_SCHEME,              NO,  BEActGrafHead  },
    { 0,             0,                       NO,  BEActSubsHead  },
    BE_ACTION_END,
};

be_action const BE_PLAN_GET[] = {
    //  Pre-checkout baseline snapshot must happen BEFORE any row
    //  that may mutate the wt (sniff get below).  Capture the
    //  current wtlog tail so BEActSubsGet can diff baseline vs
    //  target for removed/renamed subs.
    { 0,                  0,             NO,  BEActPromoteRef     },
    { 0,                  0,             NO,  BEActGetBaseline    },
    { URI_PATH,           URI_AUTHORITY, NO,  BEActWorktreeAnchor },
    { URI_PATH|URI_QUERY, URI_AUTHORITY, NO,  BEActSingleFileGet  },
    //  GET's bootstrap is unconditional — `be get be://host?/proj`
    //  derives the project name from the query and creates the
    //  shard before the wire fetch.  Distinct from PUT/POST/DELETE
    //  where bootstrap is local-only.
    { 0,                  0,             NO,  BEActBootstrap      },
    //  Fire keeper get only on transport URIs (scheme present);
    //  cached `//host` form reads local cache without opening the
    //  wire (https://replicated.wiki/html/wiki/URI.html §"Schemes — cached vs transport", Bug 2).
    //  Mirrors BE_PLAN_PATCH row at line ~287.
    { URI_SCHEME|URI_AUTHORITY, 0,        NO,  BEActKeeperGet      },
    //  Bare scheme-only transport (`ssh:` / `file:` — authority AND
    //  path both absent, GET-002 part 2): keeper completes it from the
    //  recentmost same-scheme get/post row.  Projectors are filtered
    //  out before the plan, so URI_SCHEME here is always a transport.
    //  Excluding URI_PATH leaves a path-bearing local `file:<store>`
    //  sibling-worktree form on the checkout pipeline below.
    { URI_SCHEME,      URI_AUTHORITY|URI_PATH, NO,  BEActKeeperGet  },
    { URI_AUTHORITY,   0,                      YES, BEActSpotGet    },
    { URI_SCHEME,      URI_AUTHORITY|URI_PATH, YES, BEActSpotGet    },
    { URI_AUTHORITY,   0,                      YES, BEActGrafGet    },
    { URI_SCHEME,      URI_AUTHORITY|URI_PATH, YES, BEActGrafGet    },
    { 0,               0,                      YES, BEActSniffGet   },
    { 0,               0,                      NO,  BEActSubsGet    },
    BE_ACTION_END,
};

be_action const BE_PLAN_POST[] = {
    { 0,             0,             NO, BEActPromoteRef    },
    { 0,             0,             NO, BEActPathFormCheck },
    //  Bootstrap on local-only POSTs (`be post 'msg'` on a fresh
    //  dir is the canonical init+first-commit path).  Remote-only
    //  POSTs (`be post //origin`) have their store already.
    { 0,             URI_AUTHORITY, NO, BEActBootstrap     },
    //  Pre-order submodule recursion: each sub commits before the
    //  parent, then the parent's `be put <sub>` row bumps the
    //  gitlink so the parent's commit picks up the new tip.
    { 0,             0,             NO, BEActSubsPost      },
    { 0,             0,             NO, BEActSniffPost     },
    { URI_AUTHORITY, 0,             NO, BEActKeeperPush    },
    //  Post-pass: refresh spot+graf against the new tip so future
    //  `be log:` / `be spot:` see the just-committed work.  Self-
    //  gates on dry-run (no msg, no URIs).
    { 0,             0,             NO, BEActReindex       },
    BE_ACTION_END,
};

be_action const BE_PLAN_PATCH[] = {
    { 0,                       0,             NO, BEActPromoteRef    },
    { 0,                       URI_AUTHORITY, NO, BEActBootstrap     },
    { URI_SCHEME|URI_AUTHORITY, 0,            NO, BEActKeeperGet     },
    { URI_AUTHORITY,           0,             NO, BEActResolveRemote },
    { URI_AUTHORITY,           0,             NO, BEActGrafGet       },
    //  Resolve the local source ?ref to the canonical context-free form
    //  `?/proj/branch/hash` (URI.mkd §"Resolution boundary") so sniff AND
    //  the sub-recursion both consume a fully-resolved URI — no `./feat`
    //  relative ref ever reaches a sub-worker with a different cur.
    { 0,                       URI_AUTHORITY, NO, BEActResolveRef    },
    { 0,                       0,             NO, BEActSniffPatch    },
    //  After the local absorb, recurse into subs whose gitlink pin the
    //  patch moved (gitlink-diff driven).  SUBS-002: this runs on the
    //  transport form too — `be patch ssh://host?adv` (git pull --squash)
    //  is exactly when a gitlink bump arrives and the sub must be re-got.
    //  By the time this row fires, BEActResolveRemote has rewritten the
    //  fetched URI to a local `?<sha>` form (scheme/authority cleared),
    //  so BEActSubsPatch consumes a clean local source ref regardless of
    //  the original transport.  No exclude_mask — the action's own
    //  scheme/path/query guards gate it.
    { 0,                       0,             NO, BEActSubsPatch     },
    BE_ACTION_END,
};

be_action const BE_PLAN_PUT[] = {
    //  Mutually exclusive arms.  With an authority slot, FF-push
    //  (or non-FF — PUT is unconstrained); without one, auto-
    //  bootstrap + stage in sniff.
    { URI_AUTHORITY, 0,             NO, BEActKeeperPushForce },
    { 0,             URI_AUTHORITY, NO, BEActBootstrap  },
    //  PUT-001: a path-scoped `be put <sub>/<path>` relays into the
    //  mounted sub's own put (and drops the routed args from c->uris)
    //  BEFORE the parent sniff put runs.  Gated to the path form
    //  (require URI_PATH, exclude URI_AUTHORITY) — the push form skips it.
    { URI_PATH,      URI_AUTHORITY, NO, BEActSubsPut    },
    { 0,             URI_AUTHORITY, NO, BEActSniffPut   },
    //  Bare `be put` recurses stage-all into mounted subs.
    { 0,             URI_AUTHORITY, NO, BEActSubsRelay  },
    BE_ACTION_END,
};

be_action const BE_PLAN_DELETE[] = {
    { URI_AUTHORITY, 0,             NO, BEActKeeperDelete },
    { 0,             URI_AUTHORITY, NO, BEActBootstrap    },
    { 0,             URI_AUTHORITY, NO, BEActSniffDelete  },
    //  Bare `be delete` recurses delete-all-missing into mounted subs.
    { 0,             URI_AUTHORITY, NO, BEActSubsRelay    },
    BE_ACTION_END,
};
