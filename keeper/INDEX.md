#   keeper — module index

keeper is beagle's local git object store and git-wire endpoint. A *keeper* owns the pack logs + LSM index runs of one project shard, resolves objects (chasing OFS/REF_DELTA chains), walks their trees, and speaks upload-pack / receive-pack on both ends. The store is a singleton: helpers reach `KEEP` and `HOME` directly. Branches are real dirs; writes land in the leaf, reads see the inherited chain. Prose in [KEEP.h], [LOG.md], [WIRE.md].

##  Store core

###  KEEP.h — the object-store singleton

The whole keeper surface: branch-aware open, object get/put/has, the incremental pack writer, packfile ingest, ancestry predicates, tip/remote enumeration. `KEEP` and its `keeper` struct (registries, scratch, leaf lock) are declared here.

 -  `KEEP`/`keeper`/`KEEP_DIR_S` — the singleton, its state record (registries, `buf1..buf4`, `lock_fd`), the `.be` dir.
 -  `KEEPOpen`/`OpenBranch`/`Close`/`KEEPCompact` — open (walk trunk → leaf), close+unmap, merge runs to the 1/8 cap.
 -  `KEEPCreateBranch`/`SwitchBranch`/`BranchDrop` — materialise a leaf, re-target an open keeper (DATA→PAST past LCA), drop.
 -  `KEEPInitShard`/`KEEPInitRemoteShard` — lay down a fresh project shard or per-host remote skeleton, idempotently.
 -  `KEEPGet`/`GetExact`/`GetSize`/`KEEPHas`/`Lookup` — retrieve a body by hashlet (delta resolved) or SHA-1, peek, raw val.
 -  `keepKeyPack`/`KeyType`/`KeyHashlet`/`PackBmVal`/`BmCount`/`BmLen` — inline (un)pack of the wh64 key + bookmark val.
 -  `keep_run_count_all`/`run_at_all`/`scan_branch_dir` — the PastData LSM-run enumerator and the registry-extend op.
 -  `KEEPUpdate`/`KEEPPut`/`Import`/`IngestFile`/`IngestStream` — feed one object, a batch, a `.pack`, a pack, a side-band.
 -  `keep_pack`/`KEEPPackOpen`/`PackFeed`/`PackClose` — the incremental pack writer; `PackFeed` deltas a base.
 -  `KEEPIsAncestor`/`SharesAncestor`/`CommitTreeSha`/`ObjSha` — bounded-BFS FF/shared-history predicates, commit→tree, sha.
 -  `KEEPEachTip`/`KEEPEachRemote`/`keep_tip`/`keep_remote` — iterate local tips and remote-tracking rows; may `REFSSTOP`.
 -  `KEEPMoveCommits`/`KEEPPush`/`GetRemote`/`ForEachCapToken` — cross-branch promote, push, wire fetch, cap token-loop.

##  Refs & resolution

###  REFS.h — ULOG-backed ref/tip reflog

The project's branch/tag/remote tips, stored as `dog/ULOG` rows (`<ts>\tset\t<ref-key>#?<sha>`). Resolution is one reverse pass matching host + refname; most-recent wins (format in [REF.md]).

 -  `ref`/`REFMatch`/`REFKeyCmp` — the `{time,key,val,type}` row + its key-equality / key-ordering helpers (val=`?<40-hex>`).
 -  `REFSAppend`/`AppendVerb`/`CompareAndAppend`/`SyncRecord` — append one (key→sha) row (explicit verb, or a CAS).
 -  `REFSResolve`/`REFSSourceScheme` — reverse-scan a URI to its terminal sha (filling origin), recover the clone source.
 -  `REFSLoad`/`REFSEach`/`EachRecord`/`REFSCompact` — load / iterate latest-per-key or every row, rewrite keeping latest.
 -  `REFSVerbGet`/`Post`/`Delete`/`GetFail`/`PostFail`/`Set` — cached RON60 verbs; `delete` tombstone, `*Fail` aborts.
 -  `refkind`/`REFSQueryKind` — inline probe classifying a query into `TRUNK`/`BRANCH`/`DETACHED`; never `TAG`.

###  RESOLVE.h — user-input → canonical sha/URI funnel

The single interpretation boundary: everything downstream sees only full 40-hex shas and absolute branch paths. RESOLVE turns a raw token or URI into that form.

 -  `KEEPResolveRef`/`KEEPResolveHex` — resolve one token (sha, hashlet prefix, or branch path) to a 40-byte sha or hex.
 -  `REFSResolveURI`/`KEEPResolveURI` — DIS-025 text-in/out funnel: REF arm canonicalises a query, funnel recomposes.

##  DAG & tree walking

###  WALK.h — tree walker on a KEEP store

Depth-first traversal of one tree rooted at a SHA-1 (commit walks live in graf/). The visitor `walk_tree_fn` gets `(path, kind, esha, blob, ctx)` and steers via `WALKSKIP`/`WALKSTOP` (KEEP-001).

 -  `walk_tree_fn`/`WALKu8sModeKind`/`WALK_KIND_*` — the DFS visitor typedef + mode classifier (REG/EXE/LNK/SUB/DIR).
 -  `WALKTree`/`WALKTreeLazy` — eager (blobs up front) vs lazy (pulls via `KEEPGetExact`); sniff/graf drive the lazy form.
 -  `KEEPLsFiles`/`KEEPTreeDescend` — ls-files over a URI tree, and the segment-by-segment descent behind `tree:`/`blob:`.
 -  `KEEPTreeULog`/`KEEPTreeDiff` — materialise a tree's leaves as sorted ULOG rows, and the tree-vs-tree diff.
 -  `KEEPEmitCommitLine`/`EmitTreeDiffFiles`/`EmitCommitsSince` — the "what moved" banner (commit rows, file diffs, range).

###  SUBS.h — submodule enumeration

One-level submodule listing off a tree: cross-references `.gitmodules` declarations against live `160000` gitlinks.

 -  `KEEPSubsAt` — emit one ULOG row per declared+present sub, `<url>?<mount>#<pin>`, in `.gitmodules` order; `out` reset.

##  Git wire protocol

###  REFADV.h — refs advertisement

Builds the refs advertisement: one resolved `(tip, refname, dir)` per local-tip ref key across open branch dirs, remembering each tip's source dir so negotiators map a want sha back without rescanning `REFS`.

 -  `refadv`/`refadv_entry`/`a_refadv` — the advertisement record, its `{tip, refname, dir}` entry, the declare macro.
 -  `REFADVOpen`/`Close`/`TipDirs`/`REFADVEmit` — populate from `REFS`, reset, reverse-lookup a tip's dir(s), emit advert.

###  WIRE.h, PSTR.h — upload-pack server + client + stitcher

`WIRE` negotiates wants/haves and builds the ordered segment list; `PSTR` re-frames stripped on-disk pack bodies into one wire-shaped packfile. The header also drives the client (fetch / push). See [WIRE.md].

 -  `wire_req`/`WIRE_CAP_*`/`WIRE_MAX_WANTS`/`MAX_HAVES` — the parsed request (want/have shas + caps bitmask) and limits.
 -  `WIREReadRequest`/`BuildSegments`/`ServeUpload` — drain wants/haves, resolve to an ordered `pstr_seg` list, serve.
 -  `pstr_seg`/`PSTRWrite` — a `{fd, offset, length, count}` segment and the stitcher (PACK header + bytes + trailer).
 -  `WIREFetch`/`FetchAll`/`WIREPush`/`PushDelete`/`ServePath` — client convos: fetch one/all, push (FF/`force`), delete.
 -  `WIREClassify`/`wire_evt`/`wire_role`/`wire_evt_kind` — pure pkt-line classifier → a typed want/have/done/ref event.
 -  `WIRECLFL`/`CLNRF`/`CLNFF`/`UNRCH`/`NOTRP`/`TITLECLSH` — client outcomes: fail, not-advertised, non-FF, etc.

###  RECV.h — receive-pack server

The push-direction mirror of WIRE's serve: drains ref-update commands + the packfile, hands it to `KEEPIngestFile`, FF-checks each update, appends accepted ones to `REFS`, reports status.

 -  `recv_req`/`recv_update`/`recv_result`/`RECV_CAP_*` — the parsed request (updates + caps + pack `tail`) + outcome.
 -  `RECVReadRequest`/`CloseReq`/`IngestPack`/`ApplyUpdates`/`EmitResponse`/`Serve` — parse, ingest, FF-check, append.
 -  `RECVCaptureWtPath`/`AdvanceColocatedWt` — after an accepted FF push, fork `be get ?` to advance the colocated wt.

##  Packfile indexing

###  UNPK.h — single-pass packfile indexer

Resolves every object's SHA-1 in a mapped pack — building the OFS/REF delta forest, DFS-applying deltas, falling back to `KEEPGet` for thin bases — appending one `wh128` per object for the caller to sort/dedup into a run.

 -  `unpk_in`/`unpk_stats`/`unpk_emit_fn` — the input descriptor (mapped pack, range, file_id, cb), counters, typedef.
 -  `UNPKIndex` — index one pack into `out` (unsorted, undeduped; may emit > `count` pre-dedup entries).

##  View projectors

###  PROJ.h — keeper-owned view projectors

The `tree:`/`commit:`/`blob:`/`sha1:` handlers: each takes a pre-parsed URI and emits a view — raw text or a `dog/HUNK` TLV record for `bro` — dispatched via `DOG_PROJECTORS`.

 -  `KEEPProjTree`/`ProjCommit`/`ProjBlob`/`ProjSha1` — list a dir, render a commit, emit blob bytes (TLV), emit a SHA-1.
 -  `KEEPProjDispatch` — route `u->scheme` to the handler (`PROJNONE` for a claimed-but-unwired scheme).

[KEEP.h]: ./KEEP.h
[LOG.md]: ./LOG.md
[REF.md]: ./REF.md
[WIRE.md]: ./WIRE.md
[README.md]: ./README.md
[Projector.html]:
https://replicated.wiki/html/wiki/Projector.html
