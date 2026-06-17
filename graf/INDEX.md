#   graf — module index

graf is beagle's commit-DAG / weave / projection layer over the keeper object store. It owns no bytes: it INDEXES keeper's commit, tree and blob objects into an LSM of 60-bit-hashlet edges, and PROJECTS that graph as diff / blame / log / merge / rebase views. It is a dog (`DOGOpen`/`Exec`/`Close`) and never shells to git; the only source is keeper. `MODVerbStuff` functions; the `wh128` is dog's. Prose in [GET.md].

##  Module core

###  GRAF.h — singleton state, lifecycle, projection API

The `graf` control struct (one `GRAF` global) holds the project-shard lock, the hunk-staging arena, reused object buffers, and the puppy stack of `.graf.idx` runs; every verb dispatches here. Rendering flows through `dog/HUNK.h` off the global `HUNKMode`.

 -  `GRAFOpen`/`OpenBranch`/`SwitchBranch`/`Close`/`GRAFExec` — the DOG lifecycle: open (rw) on trunk/branch, relabel, run.
 -  `GRAFRuns`/`GRAFRefreshView`/`GRAFPupCreateNext` — expose + rebuild the `wh128css` view over `.graf.idx`, mint a run.
 -  `GRAFIndex`/`GRAFIndexFromTips` — full reindex over every keeper commit vs the incremental `COMMIT_PARENT` tip-walk.
 -  `GRAFResolveTip`/`ResolveVersion`/`RefIsName`/`GRAFLca` — URI resolution: tip SHA, `?query` rewrite, name probe, LCA.
 -  `GRAFGet`/`GRAFMerge`/`MergeWtFile`/`MergeWtFileTunable`/`Merge3Bytes` — single-tip fetch + the 3-way merge fronts.
 -  `GRAFLog`/`GRAFHead`/`GRAFMap`/`GRAFBlame` — the history projectors: log, message search, subway-map, blame.
 -  `GRAFWeaveDiff`/`Diff2Layer`/`DiffWtFile`/`DiffWtTree`/`DiffTreeRefs` — the `diff:` engine: 2-layer diff, wrappers.
 -  `GRAFFileWeave`/`RebaseFileWeave`/`RebaseBlobMerge` — build a file's token weave from blob history + the rebase merge.
 -  `GRAFEmitDiffUri`/`PackUriDiffSha`/`EmitCommitUri`/`PackUriCommitSha`/`GRAFHunkEmit` — pack a link + emit.
 -  `GRAFFAIL`/`NONE`/`NOAT`/`FULL`/`NOPATH` — error codes: fail, no-resolution, no `--at`, buffer small, no branch-dir.

##  Commit-DAG index

###  DAG.h — the wh128 edge LSM and graph navigation

graf's index is an LSM of `wh128` records (two `wh64` halves, a 60-bit hashlet + 4-bit type) covering commit→parent, commit→root-tree, and `(tree,name)`→child edges. Materialising tree edges once per distinct tree lets a path descend in-memory.

 -  `DAGPack`/`DAGType`/`DAGHashlet`/`DAGEntry` — pack a `(type, hashlet)` into a `wh64`, decode it, assemble a `wh128` edge.
 -  `DAG_T_COMMIT`/`TREE`/`BLOB`/`FOSTER`/`PICKED` — per-half type nibbles; the type pair names the edge kind.
 -  `DAG_EDGE_PARENT`/`FOSTER`/`PICKED` — the reachability bitmask for the tunable walkers; picked targets one-step.
 -  `DAGRange`/`DAGLookup` — per-run binary-search over the newest-first stack: the equal-key span, or first match.
 -  `DAGCommitTree`/`DAGChildStep`/`DAGParents` — per-edge navigators: root-tree, one child, parents.
 -  `DAGAncestors`/`OfMany`/`Tunable`/`OfManyTunable`/`EdgesOf`/`AllCommits` — BFS the commit graph into a `Bwh128` set.
 -  `DAGAncestorsHas`/`dag_anc_put` — membership test and insert on a set from the ancestor walks (a `wh128` hash set).
 -  `DAGTopoSort`/`DAGTopoSortTunable` — order a commit set parents-first; the tunable form waits for masked targets.
 -  `DAGFAIL`/`NOROOM`/`NONE`/`AMBIG` — index/out-of-room error, and the no-edge / collision outcomes (keeper fallback).

###  BLOB.h — (commit, path) → object bytes via keeper

The shared resolution helpers BLAME and GET both need: pull a file's bytes at a commit using graf's 60-bit hashlets, descending the tree through keeper without an index.

 -  `GRAFTreeStep`/`GRAFPathDescend` — descend one tree entry by `name`, or a whole path, replacing `*cur` (`KEEPNONE` miss).
 -  `GRAFBlobAtCommit` — resolve `(commit_hashlet60, filepath)` to the blob: fetch commit, parse tree, walk the path.

##  Token weave

###  WEAVE.h — single-weave file history (SCCS-weave)

A file's whole history is ONE interleaved-delta TLV stream: `T` records carry token bytes + a `(seq, pos)` birth-id; `R` records carry the remover-seq set. A token is alive iff its R-set is empty. The whole DAG replays into one weave — no separate merge.

 -  `weave`/`WEAVEInit`/`Reset`/`Free`/`Empty` — the weave instance (a single `u8b tlv`), its lifecycle / empty test.
 -  `weavebld`/`BldInit`/`BldPut`, `weavecur`/`CurInit`/`CurNext`/`SetHas` — write + read.
 -  `WEAVEFromBlob`/`WEAVEFromBlobRm` — build a one-version weave from blob bytes (`ext`); `Rm` adds a remover.
 -  `WEAVEDiff`/`WEAVEApply` — the two replay front-ends: `Diff` on alive tokens (linear); `Apply` on a parent-closure (DAG).
 -  `weavedec`/`DecInit`/`DecReset`/`DecFree`/`DiffCarry` — a persistent decode carried across a long replay (GET-001).
 -  `WEAVEAliveBytes`/`EmitDiff`/`EmitFull`/`EmitMerged` — render: alive bytes, diff, change-tagged, conflict merge.
 -  `WEAVEFallbackEdl` — wholesale DEL+INS fallback when `BRAMu64s` can't fit a refined list (`DIFFNOROOM`).
 -  `WEAVE_WT_SRC`/`CFLCT_SRC`/`REC_T`/`REC_R`/`SET_MAX`/`WEAVEFAIL` — `seq` sentinels, `T`/`R` letters, cap, error.

##  Diff refinement

###  BRAM.h, NEIL.h — patience alignment and cleanup

The two passes that sharpen a raw token-level Myers edit list (`e32g` EDL) before the weave folds it: line-coherent anchoring, then whitespace/boundary cleanup. Both work on packed `u32` tokens, driven by `WEAVE.c` (and legacy `JOIN.c`).

 -  `BRAMu64s` (BRAM.h) — Bram Cohen patience diff over u64 token-hash arrays: anchor on unique lines, recurse between.
 -  `NEILCleanup`/`NEILShift`/`NEILCanon` (NEIL.h) — semantic EDL cleanup: drop false EQs, slide, collapse to INS+DEL.
 -  `NEILIsWS`/`NEIL_MAX_KILL`/`NEILBAD` (NEIL.h) — the whitespace-token test, the killable-EQ knob, the error code.

##  Merge & rebase

###  JOIN.h — legacy 3-way token merge (DEPRECATED)

The historic standalone 3-way merge: tokenize base/ours/theirs, RAP-hash, Myers-diff, walk lockstep. Superseded by WEAVE; retained only so `graf/test/JOIN01` characterises the old behaviour.

 -  `JOINfile`/`JOINTokenize`/`JOINFree` — the per-side tokenized file (data, `tok32`, `RAPHash`); used by JOIN01.
 -  `JOINMerge` — the `deprecated` merge itself; use `GRAFMerge3Bytes` / `GRAFMergeWtFileTunable` instead.
 -  `JOIN_RM_O`/`RM_T`/`IN`/`MARK`/`HASH` — the top-bit hash markers (rm-ours/theirs, inserted) + mask/strip macros.

###  REBASE.h — linear-history replay primitives

Two keeper-read-only pieces feeding the POST rewrite: a stable per-commit patch-id for cherry-pick dedup, and the replay loop. No DAG dependency, no keeper mutation; persistence via a callback.

 -  `GRAFPatchId` — a stable u64 hash of a commit's per-file delta vs its first parent; 0 for root/empty (dedup skips).
 -  `graf_rebase_emit_cb`/`GRAFRebase` — replay every commit between `base_old` and `child_tip` onto `base_new`, deduped.
 -  `GRAFCNFL` — the rebase-abort 3-way-merge-conflict code.

##  Projectors (`.c` only)

The verb implementations have no public header — each is reached through `GRAFExec` (`graf/GRAF.exe.c`) and writes hunks via `GRAFHunkEmit`: `GET.c`, `BLAME.c`, `LOG.c`, `DIFFREF.c`, `MAP.c`, `MERGE.c`, and `INDEX.c`; `DAG.c` holds the ingest state.

[GET.md]: ./GET.md
[DOG.md]: ../dog/DOG.md
