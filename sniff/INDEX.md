#   sniff — module index

sniff is the working-tree verb layer: it implements the `be` verbs `get`/`post`/`put`/`delete`/`patch`/`cat`/`ls`/`watch` over a keeper store and a `.be` worktree (prose: [AT.md], [STAGE.md]). State is ONE append-only ULOG at `<wt>/.be/wtlog`; files-on-disk + the ULOG are ground truth. Each written file is futimens-stamped, so `mtime ∈ stamp-set` means "clean". `SNIFFAt…` is the facade.

##  Core & context

###  SNIFF.h — singleton state, the ULOG-row merge engine

The `sniff` singleton holds the open `.be/wtlog` (mmapped + a ts→offset index) and the per-invocation CLI flags; `SNIFFOpen`/`Close` bracket it. The N-way merge walker is the shared primitive every classify/commit path heap-feeds into.

 -  `sniff`/`SNIFF`/`SNIFFOpen`/`SNIFFClose` — the singleton (log, ts index, flags, `.gitignore`); RW/RO open + close.
 -  `SNIFFExec`/`SNIFF_VERBS`/`SNIFF_VAL_FLAGS` — the verb dispatcher + the verb/value-flag tables for `CLIParse`.
 -  `SNIFFMergeWalk`/`sniff_step_fn` — heap-walk up to `LSM_MAX_INPUTS` ULOG cursors, one callback per path-key.
 -  `SNIFFMaybeSwitchKeeper`/`SNIFFMaybeSwitchGraf` — switch the open keeper/graf to a named branch shard if it exists.
 -  `SNIFFWtlogPath`/`SNIFFFullpath` — resolve the wtlog path by `.be` shape (dir/file), join reporoot + a relative path.
 -  `SNIFFSkipMeta`/`SNIFFRelFromFull` — YES iff a path is `.be` metadata, and turn a scan abs path into a relative slice.
 -  `SNIFFFAIL`/`SNIFFDRTY`/`SNIFFOVRL`/`SNIFFNOFF`/`CLOCKBAD` — failure, dirty, overlay-refusal, non-FF, clock-bad.

###  AT.h — the `.be/wtlog` ULOG facade

Every read/write of the worktree log goes through `SNIFFAt…`: append a row, classify a file by its mtime stamp, find the baseline / cur-tip row, and materialise the wt as a sortable ULOG stream. The stamp-set is the attribution mechanism.

 -  `SNIFFAtAppend`/`SNIFFAtAppendAt` — append one row at `RONNow()` or an explicit ts (fresh stampers need it).
 -  `SNIFFAtVerbGet`/`Post`/`Patch`/`Put`/`Delete`/`Mod`/`Repo` (`VerbOf`) — cached ron60 verbs; `VerbOf` drains a name.
 -  `SNIFFAtBaseline`/`AtCurTip`/`AtBaselineTreeSha` — pick the latest `get`/`post`/`patch` row, resolve to a tree sha.
 -  `SNIFFAtRepo`/`AtTailOf`/`AtAnchorRef`/`WtRepoAnchor` — read row 0, compose the `--at` tail, write a one-row anchor.
 -  `SNIFFAtKnown`/`AtRowAtTs`/`AtQueryFirstSha` — per-file: mtime in stamp-set, the owning row, sha from a baseline URI.
 -  `SNIFFAtPatchChain`/`AtPatchEntries`/`sniff_pe` — collect the in-scope `patch` rows' `theirs` shas, with scope + flag.
 -  `SNIFFAtLastPostTs`/`AtPatchFloorTs`/`AtScanPutDelete` — the commit-scope floors + the scan of staged rows since post.
 -  `SNIFFAtScanDirty`/`CheckClock`/`AtNow`/`AtStampPath` — walk unattributed files, the clock guard, sampler + stamp writer.
 -  `SNIFFWtULog`/`AtResolveRelativeURI`/`AtPathBytes` — materialise the wt stream, pre-resolve `?./X`, pick path.

###  CLASS.h — baseline ⊕ worktree path classifier

The read-only chokepoint for "is this path tracked / untracked / on disk / both?". It heap-merges the baseline-tree and wt streams through `SNIFFMergeWalk`, one step per distinct path — the same merge POST commits through. `be`, `be ls:`, spot read it.

 -  `SNIFFClassify`/`class_cb`/`class_step` — drive the merge and fan each path with its baseline / wt / staged records.
 -  `class_kind` (`BASE_ONLY`/`WT_ONLY`/`BOTH`) — base vs wt presence for the step.
 -  `CLASSWtEqBase` — YES iff the on-disk bytes hash to the baseline blob sha; the touched-unchanged test.
 -  `CLASSWtState`/`class_wt_state` (`CLEAN`/`PATCHED`/`MODIFIED`) — truth for `BOTH` (DIS-023).

##  Mutating verbs

###  GET.h — checkout

`be get` resolves a baseline tree, two-input-merges it against the target to classify each path, refuses on dirty ∩ change, materialises, prunes, appends one `get` row. Trunk-state `?#<sha>` (committable) vs DETACHED `?<sha>` was fixed in GET-023/024.

 -  `GETCheckout` — the core checkout: walk a commit's tree by sha prefix into the wt, stamping each write.
 -  `SNIFFGetURI`/`SNIFFGetSummary`/`SNIFFCheckout` — dispatch a `be get` shape; bare `be get` prints tips; `checkout <hex>`.
 -  `GETStatusCommitDiff`/`GETLocalBranchTip` — `be status` ahead/behind: emit one `commit:?<sha>` per differing commit.

###  POST.h — commit & promote

Two phases: commit staged/dirty content as one single-parent commit on cur (selective vs commit-all, DIS-010), then optionally ff-or-rebase-promote onto a named branch. Detached wts refuse (DIS-009); provenance is decided HERE, not at PATCH (DIS-031).

 -  `POSTCommit` — the commit path: classify, pre-hash, build trees, emit one pack, advance REFS, stamp, append `post`.
 -  `POSTPromote`/`POSTResolveBranchTip` — no-msg `be post ?<X>`: route shapes to a REFS-only promote; resolve a tip.
 -  `POSTPatchDefaults`/`POSTPrintStatus` — compose the default message/author from `patch` rows, and the dry-run print.
 -  `POSTFpChainTo`/`POST_MIG_MAX` — first-parent chain walker used by cross-shard FF migration and PUT's ref reset.
 -  `POSTDET`/`NONE`/`CFLCT`/`NOFF`/`BANG`/`NOMSG`/`NOBRANCH` — detached, none, conflict, non-FF, `!`, no-msg, no-branch.

###  PUT.h, DEL.h — stage & ref-write

PUT and DELETE are append-only intent: one `put`/`delete` row per URI, no pack/tree work — POST resolves the tree at commit time. The branch-form of each also writes a REFS row directly.

 -  `PUTStage`/`DELStage` — append one `put` / `delete` row per URI (path-form, incl. move `put <old>#<new>`, auto-pair).
 -  `PUTCreateBranch`/`PUTSetBranch`/`PUTSetLabel` — branch-form: label at tip, set a ref OUTRIGHT, append a row.
 -  `DELBranch` — drop a label via a tombstone REFS row; refuses on active descendants (unless `recursive`) or the wt's own.
 -  `PUTAMBIG`/`PUTDUP`/`PUTNOSRC`/`PUTDSTBAD`/`PUTNODIR`/`PUTMVMETA`/`PUTNONE`/`DELDIRTY` — auto-pair / move errors.

###  PATCH.h — 3-way worktree merge

`be patch ?<target>` runs a graf 3-way merge (`base=LCA`, `ours=cur.tip`, `theirs=target`) and writes merged bytes to disk as staged content — no commit; a later `be post` picks it up. The URI now selects only SCOPE (DIS-030); provenance moved to POST.

 -  `PATCHApply`/`PATCHApplyFile` — apply a whole-wt or single-file 3-way merge, writing a `patch` row + scope.
 -  `PATCHShape`/`SCOPE_NEXT`/`SCOPE_WHOLE`/`SCOPE_NAMED` — classify: `?br` one commit, `?br!` whole, `#sha` named.
 -  `SNIFFHasConflictMarker` — YES on a complete ours/base/theirs conflict-marker triple (POST + PATCH classifier).
 -  `PATCHCFLCT`/`DIRTY`/`URELT`/`PATCHBUSY`/`PATCHDET`/`PATCHFAIL` — conflict, dirty, no-ancestor, busy, detached.

##  Read verbs

###  CAT.h, LS.h — projectors

Read-only producers that emit `HUNK_TLV` records (presentation is dog/HUNK's job). `cat:` shows one file with token-diff hili vs a baseline; `ls:`/`lsr:` list the wt with one status hunk per entry.

 -  `SNIFFCat` — emit the wt file as one hunk, token-diffed (`I`/`D`/` `) against the baseline (or an explicit `?ref`).
 -  `SNIFFLs`/`SNIFFLsr` — one-level (subdirs → `dir`) and recursive worktree status listings via `SNIFFClassify`.
 -  `SNIFFLsBufsAcquire` — acquire the `ls:` one-level dedup buffer; exposed only for the leak-repro test.

##  Submodules

###  SUBS.h — gitlink / `.gitmodules` plumbing

Pure slice helpers plus the one driver GET calls per `160000` gitlink: parse/synthesise `.gitmodules`, derive a sub store dir, probe a mount, fetch+anchor+`be get` a sub. SUBS-020 picks the fetch source by parent kind.

 -  `SNIFFSubBasename`/`SubsParse`/`SubsParseFind`/`SubsSynth` — URL → basename, `.gitmodules` drain, the POST-side emitter.
 -  `SNIFFSubMount` — the driver: resolve the sub URL, write the `.be` anchor, fork `be get <url>#<hex>` in the mount.
 -  `SNIFFSubSrcEndsGit`/`SubCandidateGitRel` — the `.git`-parent discriminator + the relative-URL resolver.
 -  `SNIFFSubIsMount`/`SubReadTip` — read-only probes: is a sub mounted here, read its commit-tip sha for POST's gitlink.

##  Watcher

###  WATCH.h — inotify daemon

A coarse advisory daemon: one `mod <dir/>` ULOG row per directory holding dirty files since the latest baseline (POST still scans authoritatively). Per-baseline dedup folds repeated edits.

 -  `SNIFFWatch`/`SNIFFWatchStop` — fork the daemon (pidfile `.be/sniff.pid`, runs to SIGTERM), stop it (no-op absent).

[AT.md]: ./AT.md
[STAGE.md]: ./STAGE.md
