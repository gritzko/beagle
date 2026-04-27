#  keeper — git object store + compat layer

Parsers for git wire protocol (pkt-line, packfile) and git objects
(blob, tree, commit), plus keeper's append-only pack log and LSM
index.  Uses zlib for pack decompression and OpenSSL for SHA-1
object IDs.

Store layout is **sharded by branch directory** (see `README.md`
§"Storage layout" and `LOG.md`): each branch dir holds its own
`NNNNN.keeper` + `NNNNN.idx` files plus `refs` (a `dog/ULOG`
reflog — see `REF.md`) and optional `WT`.
`file_id`s are store-global sequential.  Object resolution walks
the dir chain child → parent → root; REF_DELTA bases are constrained
to the same dir or an ancestor.

##  Headers

### GIT.h — git object parsers

Types: none (output via slices).

  - `GITu8sDrainTree`    drain one tree entry (mode+name, 20-byte SHA1)
  - `GITu8sDrainCommit`  drain one commit header; empty field = body
  - `GITu8sCommitTree`   extract the tree SHA-1 from a commit body

### PKT.h — pkt-line framing

  - `PKTu8sDrain`      drain one pkt-line; returns PKTFLUSH/PKTDELIM for specials
  - `PKTu8sFeed`       feed one pkt-line (4-hex prefix + payload)
  - `PKTu8sFeedFlush`  feed a flush packet (0000)

### REFADV.h — git-protocol refs advertisement

  - `REFADVOpen`     walk every dir's REFS; collect (sha, refname, dir) tuples
  - `REFADVClose`    free arena + entries
  - `REFADVTipDirs`  reverse-lookup: which dir(s) hold this sha as a tip?
  - `REFADVEmit`     write the pkt-line advertisement (caps on first line + flush)

### PACK.h — packfile parser

Types: `pack_hdr` (version, count), `pack_obj` (type, size, delta ref).

Object types: COMMIT=1, TREE=2, BLOB=3, TAG=4, OFS_DELTA=6, REF_DELTA=7.

  - `PACKDrainHdr`     parse PACK magic + version + count
  - `PACKDrainObjHdr`  parse object type/size varint + delta base
  - `PACKInflate`      zlib-inflate compressed object data

### IGNO.h — .gitignore parser/matcher

Types: `igno_pat` (pattern + flags), `igno` (up to 256 patterns).

  - `IGNOLoad`   load .gitignore from directory
  - `IGNOFree`   free resources
  - `IGNOMatch`  check if relative path should be ignored

### SHA1.h — SHA-1 hash (sha1dc wrapper)

  - `SHA1Sum`                              one-shot 20-byte SHA-1
  - `SHA1Open` / `SHA1Feed` / `SHA1Close`  streaming hash; PSTR.c
                                           uses these to hash a
                                           stitched packfile inline

### PSTR.h — pack-stream encoder (WIRE.md Phase 2)

Stitches an ordered list of `(fd, offset, length, count)` segments
into one valid git packfile written to a single fd: fresh PACK
header (sum of segment counts), concatenated segment bytes
streamed via pread, fresh 20-byte SHA-1 trailer.  No object
scanning, no inflation — `count` and `length` come from pack
bookmarks (`keepPackBmCount`/`keepPackBmLen`).

  - `pstr_seg`   `{int fd, u64 offset, u64 length, u32 count}`
  - `PSTRWrite`  emit the stitched packfile to `out_fd`
  - `PSTRFAIL`   error code (count overflow, short read, write fail)

### WIRE.h — upload-pack want/have negotiator + client driver
                  (WIRE.md Phases 4 & 7)

Server side: reads a client request (wants/haves/caps) from a fd via
pkt-line, resolves each want sha to a (dir, end-of-pack) pair (REFADV
tip→dir lookup with LSM fallback), takes the max have-pack-end per dir
as the watermark, and emits the ordered `pstr_seg` list ready for
`PSTRWrite`.  Phase 1c covers the trunk shard only — the dir chain is
always `[trunk]`, one segment per request.

Client side (Phase 7, `WIRECLI.c`): spawns a peer via ssh
(`//host/path`) or local exec (`file:///path`, `keeper://local/path`),
drains the refs advertisement, sends wants/haves/done, ingests the
returned packfile (`KEEPIngestFile`), and appends a fresh REFS tip.
Push direction symmetrically spawns receive-pack, walks the local
commit's reachable closure, builds a v2 packfile inline, sends one
ref-update line + pack, drains unpack/per-ref status.

  - `wire_req`           parsed wants[] + haves[] + caps bitmask
  - `WIREReadRequest`    drain pkt-lines, populate wire_req
  - `WIREBuildSegments`  resolve wants/haves → ordered pstr_seg list
  - `WIREServeUpload`    one-shot: read request, build segs, write pack
  - `WIREFetch`          client: spawn upload-pack peer, ingest pack,
                          append REFS tip
  - `WIREPush`           client: spawn receive-pack peer, send pack,
                          drain status
  - `WIREFAIL` / `WIREBADREQ` / `WIRENOWANT` / `WIRENOSHA`
  - `WIRECLIFAIL` / `WIRECLINOREF`

### RECV.h — receive-pack server (WIRE.md Phase 6)

Symmetric to `WIRE.h` for the push direction.  Reads pkt-line
ref-update commands from a fd, drains the raw packfile that
follows the request flush, hands it to `KEEPIngestFile`
(UNPK-indexed + linked into the trunk shard), then verifies
fast-forward + appends each accepted update to REFS.  Per-update
results plus the unpack status are emitted back over pkt-line.
Refname → REFS-key convention: `refs/heads/<X>` → `?heads/<X>`,
`refs/tags/<X>` → `?tags/<X>`, val = `?<40-hex-new-sha>`.  Phase 6
MVP refuses ref deletion (new_sha all-zeros) with `RECVBADREF`;
full delete semantics are a follow-up.

  - `recv_req`           parsed updates[] + caps + arena
  - `recv_update`        old_sha + new_sha + refname slice
  - `recv_result`        per-update outcome (refname + ok64 result)
  - `RECVReadRequest`    drain pkt-lines, populate recv_req
  - `RECVCloseRequest`   release arena + updates array
  - `RECVIngestPack`     drain raw pack bytes from fd → KEEPIngestFile
  - `RECVApplyUpdates`   FF-check + REFSAppend per update
  - `RECVEmitResponse`   write "unpack ok"/"ng" + per-ref status + flush
  - `RECVServe`          one-shot: read request, ingest, apply, emit
  - `RECVFAIL` / `RECVNOTFF` / `RECVBADREF` / `RECVBADREQ`

### ZINF.h — zlib inflate/deflate wrapper

  - `ZINFInflate(u8s into, u8cs zipped)`  decompress zlib data
  - `ZINFDeflate(u8s into, u8cs plain)`  compress data


##  Implementation files

  - `GIT.c`     tree/commit drain parsers (~75 lines)
  - `PKT.c`     pkt-line framing (~77 lines)
  - `PACK.c`    packfile header/object/inflate (~101 lines)
  - `IGNO.c`    gitignore glob matching (~233 lines)
  - `SHA1.c`    SHA-1 via sha1dc (header-only inlines)
  - `ZINF.c`    zlib inflate/deflate (~63 lines)
  - `WALK.c`    KEEP-backed tree walker (eager + lazy)
  - `PSTR.c`    pack-stitcher streaming encoder (~85 lines)
  - `WIRE.c`    upload-pack want/have negotiator + segment list builder
  - `WIRECLI.c` client-side WIREFetch / WIREPush (transport spawn,
                advert drain, want/have/done, pack ingest / build,
                REFS update, push status drain)
  - `RECV.c`    receive-pack server (request parser + pack ingest +
                FF-check + REFS append + response emit)

### KEEP.h — branch-aware Open + per-shard state

`KEEPOpenBranch(home *h, u8cs branch, b8 rw)` registers `branch` on
the home singleton via `HOMEOpenBranch` and delegates to `KEEPOpen`.
Phase 0 accepts only the trunk (canonical branch = empty slice); a
non-trunk branch returns `KEEPNOBR`.

Per-shard state is extracted into `keeper_shard`: `branch`,
`lock_fd`, `packs[KEEP_MAX_FILES]`, `npacks`, `runs[KEEP_MAX_LEVELS]`,
`run_maps[]`, `nruns`.  The singleton `keeper` carries
`shards[KEEP_MAX_SHARDS]` + `nshards`, the home pointer, the paths
registry, and scratch buffers.  Phase 1a opens exactly one trunk
shard.

Phase 1b flattens the on-disk layout and drops the `keeper/` subdir
inside `.dogs/`: trunk pack logs live at `.dogs/NNNNN.keeper` and
index runs at `.dogs/NNNNN.idx`, where `NNNNN` is the 5-hex-char
wh64 `file_id` (20 bits).  `KEEP_PACK_EXT` = `.keeper`,
`KEEP_SEQNO_W` = 5, `KEEP_DIR` = `.dogs`.

Phase 1c adds the DAG-invariant scaffolding and the drop-a-dir
surface:

  * `keep_pack` gains `shard_idx` (always 0 in Phase 1c) — tags the
    write shard on every emitted pack.
  * `KEEPPackFeed`'s REF_DELTA path consults `keep_shard_visible`
    before emitting; a base that isn't in the write shard or one of
    its ancestors silently falls through to raw encoding.  nshards=1
    keeps the check trivially passing until Phase 2 opens siblings.
  * `KEEPBranchDrop(keeper *k, u8cs branch)` enforces the top-level
    preconditions: trunk aliases (`""`, `main`, `master`, `trunk`,
    plus their `heads/` forms) return `KEEPTRUNK`; unknown branches
    return `KEEPNOBR`.  The full "refuse on live descendant /
    live staging pack / live WT" semantics (`KEEPDIRTY`) will
    light up once Phase 2 can actually open a sibling to drop.

### WALK.h — git object graph traversal

Types: `walk` (walker state), `walk_fn` (visitor callback).

  - `WALKOpen`         open walker on a branch dir (mmaps that dir's
                       logs + index runs; ancestor dirs resolved lazily
                       via the keeper-wide lookup path)
  - `WALKClose`        close walker, unmap everything
  - `WALKGet`          get object by hashlet
  - `WALKGetSha`       get object by raw 20-byte SHA-1
  - `WALKTree`         DFS tree walk over KEEP — eager (blobs resolved), path-aware visitor
  - `WALKTreeLazy`     DFS tree walk over KEEP — lazy (blobs empty, pulled on demand)
  - `WALKu8sModeKind`  classify git tree-entry mode → `WALK_KIND_*`
  - `KEEPLsFiles`      ls-files on a URI-resolved tree (lazy walk + path prefix)
  - `KEEPTreeListLeaves`  materialise a tree's leaf entries as `(paths, meta)` —
                       newline-sep paths in lex order + parallel 21-byte
                       `{kind, sha[20]}` records.  Feeds `KEEPu8ssDrain` for
                       N-way tree merges (sniff/GET overlap pre-flight uses it).
  - Commit-graph traversal lives in `graf/`, not here.

### DELT.h — git delta instruction applier + encoder

  - `DELTApply`   apply delta instructions (copy/insert) to base object
  - `DELTEncode`  produce a git delta instruction stream for
                  (base, target).  4-byte hash index over `base` with
                  forward + bounded-backward extension.  Returns
                  DELTFAIL when the delta is no smaller than the raw
                  target (caller should emit raw instead).
                  Exercised end-to-end via `test/DELTA_ROUND.c`:
                  feeds a chain of blob versions with a hashlet60
                  hint to `KEEPPackFeed`, splices the log into a git
                  packfile, reads each version back via `git cat-file`.
                  `KEEPPackFeed` emits OFS_DELTA when the base is a
                  raw object in the same in-progress pack, else
                  REF_DELTA against whatever `KEEPGet` resolves from
                  committed runs (delta chains chased transparently).

##  CLI

  - `git-dl.cli.c`  thin CLI wrapper around `WIREFetch` (Phase 7);
                     opens an output keeper repo rw and fetches one ref
                     from a given remote URI (`file://`, `//host/...`,
                     `keeper://local/...`).
  - `KEEP.cli.c`    `keeper` binary entry-point.  Verbs (registered in
                     `KEEP_CLI_VERBS`): `get`, `put`, `post`, `status`,
                     `import`, `verify`, `refs`, `alias`, `ls-files`,
                     `sync`, `upload-pack`, `receive-pack`, `help`.
                     `upload-pack <repo-path>` is the git-protocol
                     drop-in for fetch: opens the named repo read-only,
                     advertises refs, runs the WIRE negotiator on
                     stdin/stdout (matches `git-upload-pack`'s ssh
                     contract).  `receive-pack <repo-path>` is the push
                     drop-in: opens rw, advertises refs, runs RECVServe
                     on stdin/stdout (drains pack, FF-checks updates,
                     appends to REFS, emits per-ref status).

##  Build

Library `gitcompat` (static): GIT.c PKT.c PACK.c DELT.c ZINF.c SHA1.c IGNO.c.
Library `keeplib` (static): KEEP.c KEEP.exe.c REFS.c REFADV.c WALK.c
                            UNPK.c PATHS.c PSTR.c WIRE.c
                            WIRECLI.c RECV.c.
Links: abc, ZLIB, OpenSSL::Crypto.

##  Tests

  - `test/GIT.c`    tree/commit parser tests (6 cases)
  - `test/PKT.c`    pkt-line drain/feed tests (8 cases)
  - `test/PACK.c`   packfile header/varint/inflate tests (7 cases)
  - `test/DELT.c`   DELTEncode + DELTApply round-trip
  - `test/DELTA_ROUND.c`  KEEPPackFeed with delta hints → valid git
                           packfile → `git cat-file` per version
  - `test/IGNO.c`   gitignore pattern matching tests (3 cases)
  - `test/ZINF.c`   deflate/inflate round-trip chain (20 versions)
  - `test/FETCH.c`  treadmill: clone repo via ssh git-upload-pack,
                     unpack packfile, write loose objects, verify with git
  - `test/WALK.c`   WALKu8sModeKind table + WALKTree/WALKTreeLazy on synthetic KEEP
  - `test/ROUND.c`  full round-trip: create bare repo, clone via ssh,
                     edit+commit, push back, verify files match
  - `test/REFADV.c` REFADVOpen/Emit/TipDirs round-trip on a temp
                     keeper: empty REFS, single trunk ref, multi-ref
  - `test/PSTR.c`   pack-stitcher: header-only (zero segs), single
                     segment passthrough, multi-segment concat,
                     round-trip header+SHA-1 verification, plus
                     `git index-pack` on the stitched output
                     (heads + tags), tip→dir lookup, pkt-line drain
                     verification (5 cases)
  - `test/POST.c`   `keeper post ssh://…` — synthesize a commit and
                     push it via git-receive-pack; verify remote HEAD
  - `test/WIRE.c`   want/have negotiator: empty request, single want,
                     have-ff watermark, unknown-sha rejection, capability
                     parsing, pkt-line round trip via pipe, end-to-end
                     PSTR + `git index-pack` verification (7 cases)
  - `test/UPLOADPACK.c` spawns the built `keeper upload-pack <repo>`
                     binary over pipes, drives a flush-only smoke
                     request (refs advert arrives with ≥ 1 ref) and
                     an end-to-end want/done fetch (response validates
                     via `git index-pack --stdin`).
  - `test/RECEIVEPACK.c` spawns the built `keeper receive-pack <repo>`
                     binary over pipes.  4 cases: flush-only smoke,
                     single-ref create with a real `git pack-objects`
                     pack, FF update against a seeded tip, non-FF
                     rejection (REFS unchanged, `ng …
                     non-fast-forward` on the wire).
  - `test/WIRE_CLIENT.c` end-to-end smoke for `WIREFetch` / `WIREPush`
                     against the built `keeper upload-pack` /
                     `receive-pack` binaries via `file://…`:
                     fetch smoke (server has commit + REFS, client
                     fetches → REFS holds tip), push smoke (source
                     pushes → destination REFS holds tip), round trip
                     (A → B push, A → C fetch, all three agree).
