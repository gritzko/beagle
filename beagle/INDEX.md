#   beagle — module index

beagle is the `be` command-line front-end: a thin verb dispatcher tying the dogs (bro, spot, graf, sniff, keeper) together. `be` never mutates a store; it parses a URI, canonicalizes it, classifies the verb, and spawns the right dog(s). `BEAct*` are the per-row action bodies of the dispatch table; `BESubs*`/`BEGet*Subs` drive submodule recursion. Two public headers cover dispatch + sub-spawn.

##  Dispatch

###  DISPATCH.h — declarative per-verb dispatch table

Each HTTP verb is a static array of `be_action` rows. The executor OR-folds `URIPattern` across every input URI into one aggregate, walks the table once, firing each row whose gate matches (rows fire-once). Bodies live in `DISPATCH.c` / `BE.cli.c`.

 -  `be_action`/`be_action_fn` — one table row (`require_mask`/`exclude_mask`, `parallel` hint, `fn`) + the body type.
 -  `BESTOP`/`BE_ACTION_END` — a clean short-circuit (low byte `0x19`, not `*NONE`'s `0xCE`) and the `{0,0,NO,NULL}` end.
 -  `BEExecute` — the executor: aggregate the URI pattern, clear the empty-host bit, walk `plan`; surfaces the last `*NONE`.
 -  `BE_PLAN_HEAD`/`GET`/`POST`/`PATCH`/`PUT`/`DELETE` — the six per-verb row tables; `becli_inner` picks one.
 -  `BEActPromoteRef`/`ResolveRef`/`PathFormCheck`/`ResolveRemote` — URI rewriters (no spawn).
 -  `BEActBootstrap`/`WorktreeAnchor`/`GetBaseline`/`SingleFileGet` — GET-side prep before the spawn.
 -  `BEActSniffGet`/`SniffPut`/`SniffPost`/`SniffPatch`/`SniffDelete` — sniff worktree spawns, one per verb.
 -  `BEActKeeperGet`/`KeeperPush`/`KeeperPushForce`/`KeeperDelete` — keeper store + transport spawns.
 -  `BEActSpotGet`/`GrafGet`/`GrafHead`/`Reindex` — indexer/diff spawns; `GrafHead` cached diff, `Reindex` the POST pass.
 -  `BEActSubsHead`/`SubsGet`/`SubsPost`/`SubsPatch`/`SubsRelay`/`SubsPut` — per-verb sub recursion entry points.
 -  `BESpawn`/`BEReap`/`BEBuildArgv` — non-blocking spawn+reap pair, and the batched argv composer (`SNOROOM` on overflow).
 -  `BEPromoteRef`/`BEEnsureProjectRepo`/`BEHeadSubs` — shared bodies: shift a bareword, mkdir a shard, HEAD's sub walk.

###  SUBS.h — submodule recursion plumbing

BE-side orchestration for descending into mounted submodules: enumerate one level off `<wt_root>/.gitmodules`, then fork + `chdir` + `execvp` self to re-enter the dispatcher inside the child. The mount syscalls land in `sniff sub-mount`, not here.

 -  `besub` — one declared sub: `path` (mount), `url` (`.gitmodules`), `mounted` (`<wt>/<path>/.be` is a file).
 -  `besub_cb` — per-entry callback `ok64 (*)(besub const *, void *ctx)`; slices scratch for the call, a non-OK aborts.
 -  `BESubsHere` — enumerate the subs declared in `<wt_root>/.gitmodules` (absence OK; malformed → SUBSPARSE).
 -  `BERecurseInto` — fork + `chdir(<wt>/<subpath>)` + `execvp` self; returns OK / `BEDOGEXIT` / `BEDOGSIG` (SUBS-022).
 -  `BERelaySub` — spawn `be <argv…>` (`--tlv`) in the sub mount, capture TLV, re-emit each hunk rebased.
 -  `BEIndexMount` — fork `spot get` + `graf get` with cwd at the sub mount so a locally-got sub gets indexes.
 -  `BERun` — resolve sibling `tool`, spawn, wait, map the exit into `BEDOGEXIT`/`BEDOGSIG`/`*NONE`/OK; `bg=YES` skips wait.
 -  `BEGetKeeperSubs` — spawn `keeper subs ?<query>` and capture its ULOG rows into `out` (empty `out` on no-sub, still OK).
 -  `BEGetSubMount`/`BEGetSubUnmount` — first-time mount via `sniff sub-mount … #<pin>`, unmount by unlinking `.be`.
 -  `BEGetDrainSubs` — iterate `keeper subs` rows: mount each pin, `BERelaySub` a `be get ?<pin>`, then `BEIndexMount`.
 -  `BEGetDrainRemoved` — walk the baseline ULOG and unmount any sub absent from the target ULOG (skips gone anchors).

##  Entrypoints (`*.c`)

`BE.cli.c` is the dispatcher proper. `becli`/`becli_inner` (`MAIN`) parse argv, open `&HOME`, then canonicalize every URI. A projector URI routes through `BEProjector`; a bare path views via `bro`; otherwise the verb picks one `BE_PLAN_*` for `BEExecute`.

`DISPATCH.c` holds `BEExecute`, the six `BE_PLAN_*` tables, and the action bodies. `SUBS.c` implements the `SUBS.h` surface: `BESubsHere`, the fork/exec recursors, and the `be get` drains; `BEDOGEXIT`/`BEDOGSIG` are duplicated, identical to BE.cli.c's.

[README.md]: ./README.md
