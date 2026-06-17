#   spot — module index

spot is the repo code-search and refactoring engine: a syntax-aware `cat`, structural snippet search-and-replace, indexed `grep`, Thompson-NFA regex grep, and token diff/merge — all over an inverted trigram + symbol index in `.be/*.spot.idx` (see [README.md], [NEIL.md]). spot is purely a *producer* of `hunk` records; presentation lives in `bro/`. The singleton mirrors `KEEP`/`SNIFF`/`GRAF`; `CAPO*` is the query surface.

##  Engine API

###  CAPO.h — index, search, grep, the `spot` singleton

Open/close the `SPOT` singleton, run the three query verbs, drive the LSM posting index. A `wh64` posting is `[off:40 | id:20 | type:4]`, sorts by `off`; `id` is the 20-bit full-path hash, `type` picks trigram / sym-use / sym-def / blob-memo.

 -  `SPOTOpen`/`OpenBranch`/`Close`/`SPOTExec` — open the singleton at `&HOME` (rw/ro; branch trunk-only), close, run.
 -  `CAPOSpot`/`CAPOGrep`/`CAPOPcreGrep` — the three verbs: snippet search (+replace), indexed grep, NFA grep.
 -  `SPOTIndexFromTips` — `spot get URI` ingest: walk tip trees, skip `(blob,path)` in `BLOBFN`, tokenise, emit postings.
 -  `CAPOIndexBlob`/`CAPOIndexFile`/`CAPOEmit`/`CAPOFlushRun` — tokenise a blob/file, append a `wh64`, flush a sorted run.
 -  `CAPOCompact`/`CAPOCompactAll`/`CAPOMergeWorkers` — LSM: keep the 1/8 ladder, merge runs, fold in `--fork N` runs.
 -  `CAPORefreshView`/`CAPORuns` — rebuild the live `u64cs` view over the puppy stack and publish it to a caller slice.
 -  `CAPOFnRap20`/`Tri40`/`Sym40`/`OffPrefix`/`TriChar` — pack posting fields: path key, trigram/symbol slots, RON64 gate.
 -  `CAPOResolveDir`/`CAPOKnownExt`/`CAPONextSeqno` — resolve `<root>/.be`, test a tokeniser by ext, pick the next run.
 -  `spot`/`SPOT` (`spotp`/`spotcp`) — the singleton struct (lock, puppy stack, BOX scratch, buffers) + CLI tables.
 -  `SPOTOPEN`/`OPENRO`/`NOBR`/`NOPATH`/`CAPONOROOM`/`NODIFF` — open-result and error codes (`ron60`, as keeper/graf).

###  CAPOi.h — internal scan, filter, hunk-building helpers

Not part of the published API (it pulls `PRO.h` macros): the file walkers, trigram pre-filter, and the shared hunk builder behind all three verbs. `vcall` is a fail-with-step-context wrapper.

 -  `CAPOScan`/`ScanFiles`/`ScanRef`/`CAPORunScan` — walk the worktree, a file list, or a ref's tree per `opts->file_fn`.
 -  `CAPOScanOpts`/`CAPOFileFn` — the per-scan config (ext filter, `tri_hashes`, cb) + the shared per-file callback.
 -  `CAPOTrigramFilter`/`Regex`/`CollectPaths`/`FilterInPlace` — precompute the candidate path-hash set, skip non-matches.
 -  `CAPOBuildHunk` — build one syntax-highlighted, context-bounded `hunk` from a token stream + ranges.
 -  `CAPOFindFunc`/`GrepCtx`/`Progress`/`ExtIs`/`branch_dir` — enclosing `'N'`-def, context, progress, ext, dir.
 -  `u64csSwap` + `HITu64*` (`abc/HITx.h`) — heap-of-iterators (`Merge`/`Seek`/`SeekRange`) over the trigram postings.

##  Structural matcher

###  SPOT.h — token-stream pattern matching

Flat token-pattern matching: tokenise needle and haystack, scan for the needle, where lowercase placeholders bind one token, uppercase a bracket-balanced block, and a two-space gap skips a run. Matching is structural, so formatting may differ.

 -  `SPOTTokenize` — tokenise source into a packed `u32` token buffer via `dog/tok/` (`SPOTBIG` past the 24-bit offset).
 -  `SPOTInit`/`SPOTNext` — set up a `SPOTstate` over needle + haystack, then step: `OK` per match, `SPOTEND` when done.
 -  `SPOTReplace` — apply a replacement template to every match in one file, writing rewritten bytes + the match count out.
 -  `SPOTstate`/`SPOTntok` — the matcher state (token slices, scan pos, binds, captures) + one flattened needle token.
 -  `SPOTEND`/`SPOTBAD`/`SPOTNONE`/`SPOTBIG` — match-exhausted, malformed-pattern, no-match-here, source-too-large.

###  RXLITS.h — regex literal extraction

A tiny Ragel walker that pulls the literal byte runs out of a regex so `CAPOPcreGrep` can trigram-filter the index (the Russ Cox approach) before running the NFA on survivors.

 -  `RXLITSu8sDrain`/`rxlits_cb` — walk a pattern, calling back per literal byte (`flush=NO`) and at each meta-boundary.

##  Output staging

###  LESS.h — producer-side hunk staging

Builds hunks in a 16 MB arena and serialises each to `spot_out_fd` via `HUNKu8sFeedOut`, whose framing is chosen off the module-global `HUNKMode`. Single-buffered: every emit reuses the slot and rewinds the arena.

 -  `LESSArenaInit`/`LESSArenaCleanup`/`LESSRun` — set up/tear down the staging arena + rings, and the end flush of maps.
 -  `LESSHunkEmit` — serialise the built hunk to `spot_out_fd` and rewind; propagates a write failure (e.g. `EPIPE`).
 -  `LESSDefer`/`spot_out_fd`/`LESShunk` — defer an mmap+token pair for cleanup, the outgoing-hunk fd, the `hunk` alias.

##  Source files (not headers)

The matcher and verbs split across `.c` files: `CAPO.c` (index, `CAPOSpot`, hunks), `GREP.c` (grep verbs), `SPOT.c` (the engine), `LESS.c` (staging + emit), `RXLITS.rl.c` (literal walker), and the CLI in `CAPO.cli.c` / `CAPO.exe.c`.

[README.md]: ./README.md
[NEIL.md]: ./NEIL.md
[dog/HUNK.h]: ../dog/HUNK.h
