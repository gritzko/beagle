#  keeper — git object store + compat layer

Parsers for git wire protocol (pkt-line, packfile) and git objects
(blob, tree, commit), plus keeper's append-only pack log and LSM
index.  Uses zlib for pack decompression and OpenSSL for SHA-1
object IDs.

Store layout is a **single flat object shard per project** (see
`https://replicated.wiki/html/wiki/Store.html` §"Repo dir layout"): one dir
`<store>/<project>/` holds every `NNNNN.keeper` pack log +
`NNNNN.keeper.idx` index run for *all* local branches, tags and
remote-tracking refs, plus one `refs` (a `dog/ULOG` reflog — see
`REF.md`).  There are no per-branch object subdirectories and no
separate remotes store.  Branches and tags are pure ref rows in the
single `refs`.  `file_id`s are store-global sequential.  Object
resolution consults exactly one dir — no dir-chain walk; REF_DELTA
bases resolve within the same shard.

##  Headers

The git-compat shims (`GIT.h`, `PKT.h`, `PACK.h`, `IGNO.h`, `SHA1.h`,
`ZINF.h`, `DELT.h`) all live in `dog/git/`; their sections below are
prefixed with `dog/git/` to make that explicit.  Everything else in
this section lives in `keeper/`.

### dog/git/GIT.h — git object parsers

Types: none (output via slices).

  - `GITu8sDrainTree`    drain one tree entry (mode+name, 20-byte SHA1)
  - `GITu8sDrainCommit`  drain one commit header; empty field = body
  - `GITu8sCommitTree`   extract the tree SHA-1 from a commit body

### dog/git/PKT.h — pkt-line framing

  - `PKTu8sDrain`      drain one pkt-line; returns PKTFLUSH/PKTDELIM for specials
  - `PKTu8sFeed`       feed one pkt-line (4-hex prefix + payload)
  - `PKTu8sFeedFlush`  feed a flush packet (0000)

### REFADV.h — git-protocol refs advertisement

  - `REFADVOpen`     read the project's single REFS; collect (sha, refname) tuples
  - `REFADVClose`    free arena + entries
  - `REFADVTipDirs`  reverse-lookup: which ref(s) hold this sha as a tip?
  - `REFADVEmit`     write the pkt-line advertisement (caps on first line + flush)

### dog/git/PACK.h — packfile parser

Types: `pack_hdr` (version, count), `pack_obj` (type, size, delta ref).

Object types: COMMIT=1, TREE=2, BLOB=3, TAG=4, OFS_DELTA=6, REF_DELTA=7.

  - `PACKDrainHdr`     parse PACK magic + version + count
  - `PACKu8sFeedHdr`   encode the 12-byte PACK header (magic+ver+count)
  - `PACKDrainObjHdr`  parse object type/size varint + delta base
  - `PACKu8sFeedObjHdr` encode the object type/size varint header (the
                       encode counterpart of `PACKDrainObjHdr`; shared by
                       every pack writer — keeper's pack log + push
                       builder).  Round-trip test: `test/PACK.c` case 11
  - `PACKInflate`      zlib-inflate compressed object data

### dog/git/IGNO.h — .gitignore parser/matcher

Types: `igno_pat` (pattern + flags), `igno` (up to 256 patterns).

  - `IGNOLoad`   load .gitignore from directory
  - `IGNOFree`   free resources
  - `IGNOMatch`  check if relative path should be ignored

### dog/git/SHA1.h — SHA-1 hash (sha1dc wrapper)

  - `SHA1Sum`                              one-shot 20-byte SHA-1
  - `SHA1Open` / `SHA1Feed` / `SHA1Close`  streaming hash; PSTR.c
                                           uses these to hash a
                                           stitched packfile inline
  - `SHA1u8sFeedHex` / `a_sha1hex`         canonical sha1→40-hex
                                           encode (CODE-016)
  - `sha1FromBin` / `sha1Drain`            binary (20-byte) → sha1 decode
  - `sha1FromHex`                          40-hex → sha1 decode; the one
                                           hex→sha1 funnel (CODE-016),
                                           `>=40` else BADRANGE, HEXBAD
                                           on non-hex

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
pkt-line, resolves each want sha to an end-of-pack offset in the single
project shard (REFADV tip lookup with LSM fallback), takes the max
have-pack-end as the watermark, and emits the ordered `pstr_seg` list
ready for `PSTRWrite`.  All objects live in one shard, so the segment
set comes from that single dir.

Client side (Phase 7, `WIRECLI.c`): spawns a peer via ssh
(`//host/path`) or local exec (`file:///path`, `keeper://local/path`),
drains the refs advertisement, sends wants/haves/done, ingests the
returned packfile (`KEEPIngestFile`), and appends a fresh REFS tip.
Push direction symmetrically spawns receive-pack, walks the local
commit's reachable closure, builds a v2 packfile inline, sends one
ref-update line + pack, drains unpack/per-ref status.

  - `wire_req`           parsed wants[] + haves[] + caps bitmask
  - `WIREServePath`      build the served repo-path argv element:
                          `path` + absolute `?/<project>` selector (so
                          a keeper peer routes to that shard, not its
                          row-0 default).  Both transport branches
                          (local exec + ssh) funnel through it; a bare
                          `?ref` (the want) is NOT appended.
                          (`test/SERVEPATH.c`)
  - `WIREReadRequest`    drain pkt-lines, populate wire_req
  - `WIREBuildSegments`  resolve wants/haves → ordered pstr_seg list.
                          wire_locate_sha resolves a sha to its LATEST
                          (largest-offset) copy so the want's end_offset
                          and a have's watermark track the log tail; the
                          earliest copy would re-ship duplicate-laden
                          packs (GET-007, `test/WIREREFETCH.c` /
                          `test/WIREE2EREFETCH.c`)
  - `WIREServeUpload`    one-shot: read request, build segs, write pack
  - `WIREFetch`          client: spawn upload-pack peer, ingest pack,
                          append REFS tip.  A 40-hex `want_ref` is a
                          WANT-BY-HASH: sends `want <sha>` directly
                          (bypasses advert matching), lands the pin even
                          from a zero-/wrong-refs shard, records it as
                          trunk.  Used by sniff sub-mount for the
                          gitlink pin (`test/WIRE_CLIENT.c` case 4).
                          DIS-012 title-clash gate: after ingest, a
                          branch fetch (not a by-hash pin) into a shard
                          that already holds a local project tip refuses
                          with `TITLECLSH` when the incoming tip shares
                          no common ancestor with any existing tip
                          (`KEEPSharesAncestor`); the orphaned objects
                          stay unreferenced.  Shared history converges
                          (`test/WIRE_CLIENT.c` cases 5/6).
                          DIS-028: the trunk⇔`refs/heads/main` wire alias
                          fires ONLY on the empty-want (trunk-discovery)
                          path.  The non-empty ref matcher
                          (`wcli_refname_match`) parses the ADVERTISED
                          name with `GITParseRef` (raw bare name), so an
                          explicit `?main` resolves the peer's literal
                          `refs/heads/main` like any other branch instead
                          of folding to empty and failing `WIRECLFL`
                          (`test/get/38-remote-main-branch`).
  - `WIREFetchAll`       client: single upload-pack session, multi-want
                          for every advertised heads/tags ref.  Backs
                          `be head ssh://origin?*` (https://replicated.wiki/html/wiki/HEAD.html §HEAD).
                          Capped at WIRECLI_FETCHALL_MAX (64) refs per
                          session
  - `WIREPush`           client: spawn receive-pack peer, send pack,
                          drain status
  - `WIREFAIL` / `WIREBADREQ` / `WIRENOWANT` / `WIRENOSHA`
  - `WIRECLIFAIL` / `WIRECLINOREF`
  - `TITLECLSH`          title clash: same-title clone with disjoint
                          history ([Title] §"Same title, different
                          history is an error"); resolve via
                          `be get <uri>?/<title>` override

### RECV.h — receive-pack server (WIRE.md Phase 6)

Symmetric to `WIRE.h` for the push direction.  Reads pkt-line
ref-update commands from a fd, drains the raw packfile that
follows the request flush, hands it to `KEEPIngestFile`
(UNPK-indexed + appended to the single project shard), then verifies
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
  - `RECVCaptureWtPath` / `RECVAdvanceColocatedWt` — after an FF push,
    advance the COLOCATED primary wt (`be get ?`).  POST-014: only fires
    when `h->wt` (the home's own worktree) equals the two-pop wt root —
    a central store (`h->wt == <store>/.be`) is skipped, never escaping
    to the store parent (`$HOME`); a failed courtesy advance is reported
    as a deferral, never as a raw `Error:`/non-zero push.
  - `RECVFAIL` / `RECVNOTFF` / `RECVBADREF` / `RECVBADREQ`

### dog/git/ZINF.h — zlib inflate/deflate wrapper

  - `ZINFInflate(u8s into, u8cs zipped)`  decompress zlib data
  - `ZINFDeflate(u8s into, u8cs plain)`  compress data

### RESOLVE.h — user-input → canonical sha funnel

Interpretation step at the boundary between user input and the
internals.  Downstream code only ever sees full 40-hex shas and
absolute branch paths.

  - `KEEPResolveRef`  canonicalise sha / hex-prefix / branch path /
                      relative-branch-path / (postponed) commit-msg
                      fragment into a 40-byte sha
  - `KEEPResolveHex`  same for a hex (sha-prefix / branch tip) lookup
  - `REFSResolveURI`  DIS-025 Stage 2 REF arm: raw user ref query (text)
                      → canonical resolved query text, one of three
                      structural shapes — `?/<proj>/<sha>` (TRUNK),
                      `?/<proj>//<sha>` (DETACHED), `?/<proj>/<br>/<sha>`
                      (BRANCH).  Idempotent on already-canonical input;
                      pin via KEEPResolveRef, context from `home`.
  - `KEEPResolveURI`  full funnel: REF arm + (Stage-3 TODO) path arm
                      (cwd→root) and auth arm (`//alias`→URL), which today
                      pass through verbatim; recomposes the absolute URI.
  - `REFSQueryKind`   (inline in `keeper/REFS.h`) syntactic kind probe
                      over the canonical query → `refkind`
                      NONE/TRUNK/DETACHED/BRANCH; never returns TAG (a tag
                      waypoint is byte-identical to a nested branch — the
                      tags-namespace refinement is REFS-aware, done in the
                      funnel, not the probe).

### UNPK.h — single-pass packfile indexer

Given a pack mapped in memory, resolve every object's SHA-1
(chasing OFS_DELTA / REF_DELTA) and emit one `wh128` entry per
object: `key = hashlet60 | type`, `val = flags | file_id | log_off`.
Thin-pack fallback resolves REF_DELTA bases via `KEEPGet` against
earlier packs / index runs.  Phase B forks workers that carry
COW-private `resolved[]`, so a delta reachable from two roots owned
by different workers is emitted once per worker — `UNPKIndex` may
emit MORE than `count` entries (the duplicates collapse in the
caller's post-index sort+dedup).  The `out` buffer therefore grows
on demand (`unpk_push` → `wh128bReserve`); the caller must NOT assume
a `count`-sized cap (GET-005: a 289862-object beagle clone emitted
290616 entries and a fixed `count + 16` cap tripped SNOROOM on the
trailing bookmark push in `KEEPIngestStream`/`KEEPIngestFile`).

  - `unpk_in`     mmapped pack slice + file_id + log_off
  - `UNPKIndex`   index pack → emit sorted/deduped wh128 entries
                  (grows `out`; may emit > count pre-dedup)

### PROJ.h — keeper-owned view projectors

URI handlers wired through `KEEPProjDispatch` (called from
`KEEP.exe.c` when `DOG_PROJECTORS` routes a scheme to "keeper").
TLV mode goes through `dog/HUNK` so `bro` can render the output.

  - `KEEPProjTree`     `tree:[<path>]?<ref|sha>` — directory entries.
                       Each entry's name is an `F` anchor followed by a
                       `U` URI token (`tree:<sub>/?<rev>` for dirs,
                       `blob:<path>?<rev>` for files); a `..` row leads
                       the listing when `<path>` is non-empty.
  - `KEEPProjCommit`   `commit:?<ref|sha>` — commit header + body.  The
                       `tree <sha>` and each `parent <sha>` header line
                       carries an `L` anchor on the sha + a `U` token
                       (`tree:#<sha>` / `commit:#<sha>`); other headers
                       and the message body render with default tags.
  - `KEEPProjBlob`     `blob:[<path>]?<ref|sha>` — blob bytes (TLV
                       mode tokenizes via `dog/TOK`)
  - `KEEPProjDispatch` scheme → projector dispatch entry point

`U` token convention: URI bytes follow the visible anchor token in
`hk->text`; all renderers / line classifiers / search hide them, so a
left-click on the anchor opens the next-token URI via `be --tlv`
(see `dog/tok/TOK.h`, `bro/BRO.c`).


##  Implementation files

  - `KEEP.c`     branch-aware Open + Get/Has/Lookup/Scan + pack
                 writer (PackOpen/Feed/Close) + Import/Ingest +
                 Push + tip/remote enumeration
  - `KEEP.exe.c` CLI verb dispatch (`get`, `put`, `post`, `status`,
                 `refs`, `alias`, `sync`, `upload-pack`, …).  POST-008:
                 `keeper_post`'s target gate is scheme-symmetric with
                 fetch — accepts host OR authority OR path OR `?/<proj>`
                 OR a transport scheme, so a host-less `file:///abs`
                 local store (and `be://`) is a valid push target, not
                 just ssh.  POST-009: `keeper_post` strips an absolute
                 `?/<proj>[/<branch>]` project selector
                 (DOGQueryStripProject) before using the query as the
                 push branch — the raw `/proj` was treated as a branch
                 and landed objects on a phantom `refs/heads//proj`, so
                 the peer trunk never advanced.
  - `MIGRATE.c`  `KEEPMoveCommits` — **retired**: with one flat object
                 pool per project, promote/drop are REFS-only and there
                 is no cross-shard pack copy to perform (`be post
                 ?<other>` just appends a ref).
  - `REFS.c`     URI→URI reflog (append + load + resolve)
  - `REFADV.c`   refs advertisement (read the single refs, emit pkt-lines)
  - `RESOLVE.c`  user-input → canonical sha funnel
  - `WALK.c`     KEEP-backed tree walker (eager + lazy)
  - `UNPK.c`     single-pass pack indexer (delta chase + thin-pack)
  - `PSTR.c`    pack-stitcher streaming encoder
  - `WIRE.c`     upload-pack want/have negotiator + segment list
                 builder (`WIRE.c.rl` is the ragel source for
                 `WIRE.rl.c` co-built alongside)
  - `WIRECLI.c`  client-side `WIREFetch` / `WIREPush` (transport
                 spawn, advert drain, want/have/done, pack ingest /
                 build, REFS update, push status drain)
  - `RECV.c`     receive-pack server (request parser + pack ingest
                 + FF-check + REFS append + response emit)
  - `PROJ.c`     view projectors (`tree:` / `commit:` / `blob:`)

### KEEP.h — flat-shard Open + branch-as-ref state

`KEEPOpenBranch(home *h, u8cs branch, b8 rw)` opens the single project
shard `<store>/<project>/` and registers every `.keeper` (pack log)
and `.keeper.idx` (LSM index run) file in it on the keeper-level
`packs` and `puppies` `Bkv32` registries.  `branch` is normalized via
`DPATHBranchNormFeed` and merely selects the *ref context* (which
`?heads/<name>` tip the session reads/advances) — it never names a
directory.  In rw mode, exclusive flock lands on
`<store>/<project>/.lock`.  `KEEPOpen` is a thin wrapper that passes
the empty (trunk) branch.  A **read-only open never creates
directories** (GET-010): the trunk-dir `FILEMakeDirP` is gated on `rw`,
so e.g. `keeper upload-pack` serving a clone of an empty/missing shard
cannot manufacture a stray `<store>/<project>/` shard (with placeholder
0-byte `refs`) in someone else's store on a mere read — the pack/idx
scan tolerates an absent dir, so the serve reads zero refs and the
clone fails cleanly.

The singleton `keeper` carries:
  * `home *h` — the borrowed home pointer.
  * `Bkv32 packs` — `seqno → fd` for every pack file in the project
    shard.  Lookups are linear scans; seqnos are globally unique
    across the keeper instance.
  * `Bkv32 puppies` — `seqno → fd` for every index run; iterated by
    LSM lookups (`KEEPLookup` / `KEEPGetExact`).
  * `path8b leaf_branch` — canonical current-branch ref path (trailing
    '/'; empty for trunk); heap-allocated in `KEEPOpenBranch` so it
    owns its bytes (no caller-slice borrow).  Read via `u8bDataC()`.
    Selects the ref context only; not a directory.
  * `int lock_fd` — flock on the shard's `.lock`; -1 = ro.
  * `u32 next_seqno` — `max(seqno) + 1` across both registries.
  * `Bu8 buf1..buf4` — KEEPGet scratch.

`KEEPSwitchBranch` changes the active ref context (which tip the
session reads/advances) without touching the filesystem — there are
no per-branch dirs to open or close.

`KEEPCreateBranch(home *h, u8cs branch)` creates a branch by writing
one `?heads/<name>` ref row into the single `refs` reflog; it makes no
directory.  Returns `KEEPTRUNK` on empty branch, `KEEPDUP` if the ref
already exists.

`KEEPBranchDrop(keeper *k, u8cs branch)` drops a branch by appending a
REFS tombstone for its `?heads/<name>` ref; it deletes no objects and
no directories (the objects linger for epoch-based GC).  Refuses trunk
(`KEEPTRUNK`); refuses the active branch (`KEEPDIRTY`); refuses an
unknown ref (`KEEPNONE`).

On-disk layout (one flat shard per project):
  * `<store>/<project>/`              — the only object dir; holds the `refs` reflog.
  * `<store>/<project>/NNNNN.keeper`  — pack log (10-char RON64 seqno).
  * `<store>/<project>/NNNNN.keeper.idx`  — keeper index run.
  * `<store>/<project>/NNNNN.graf.idx` / `NNNNN.spot.idx` — workdog indexes, flat.
All objects for every local branch, tag and remote-tracking ref live
here; writes append to this dir and reads consult only this dir (no
dir-chain fan-out, no per-branch or remotes subdirectories).

Shared low-level helpers exported from `KEEP.h` (CODE-004/005 dedup):
  * The hex40→sha1 decode is dog/git/SHA1.h `sha1FromHex(sha1 *out,
    u8csc hex)` (relocated here in CODE-016, next to `sha1FromBin`) —
    decode the leading 40 hex chars of a commit-header value into a
    sha1, returning `ok64` (BADRANGE on <40 bytes, else the
    `HEXu8sDrainSome` code).  It is the single hex→sha1 funnel: backs
    the parent/foster walks in `KEEP.c`, `WALK.c`, `WIRECLI.c`,
    `RESOLVE.c`, `REFADV.c`, the at-sha decode in `KEEP.exe.c`, and
    graf/spot REFS-val decodes; no keeper-local copy.
  * The pack object-header varint encoder is `PACKu8sFeedObjHdr` in
    `dog/git/PACK.h` (the encode counterpart of `PACKDrainObjHdr`) —
    keeper's pack writer (`KEEP.c`) and the push pack builder
    (`WIRECLI.c`) both call it; no keeper-local copy.
  * `KEEPForEachCapToken(u8csc tail, b8 tab_is_sep, fn, ctx)` — the
    SP/TAB/'\n' capability token-loop scaffold behind both
    `wire_parse_caps` (upload-pack, no TAB) and `recv_parse_caps`
    (receive-pack, TAB-separated); each caller's `fn` maps a token to
    its own cap-bit set.
The commit-ancestry walks (`KEEPIsAncestor`, `KEEPSharesAncestor`) are
rebuilt on three internal primitives in `KEEP.c`: `keep_commit_parents`
(iterate decoded parent/foster shas), `keep_commit_bfs` (bounded,
seen-deduped BFS with caller-supplied scratch + per-node visitor and
miss/cap policy flags), and `keep_bfs_enqueue` (dedup+enqueue adaptor).

### WALK.h — git object graph traversal

Types: `walk` (walker state), `walk_fn` (visitor callback).

  - `WALKOpen`         open walker on the project shard (mmaps the
                       shard's pack logs + index runs; all objects
                       resolve from the single keeper-wide lookup path)
  - `WALKClose`        close walker, unmap everything
  - `WALKGet`          get object by hashlet
  - `WALKGetSha`       get object by raw 20-byte SHA-1
  - `WALKTree`         DFS tree walk over KEEP — eager (blobs resolved), path-aware visitor
  - `WALKTreeLazy`     DFS tree walk over KEEP — lazy (blobs empty, pulled on demand)
  - `WALKu8sModeKind`  classify git tree-entry mode → `WALK_KIND_*`
  - `KEEPLsFiles`      ls-files on a URI-resolved tree (lazy walk + path prefix)
  - `KEEPTreeDescend`  shared '/'-separated tree-path descent (CODE-005):
                       segment-by-segment match from a root tree, optional
                       prefix `pathbuf` accounting, caller-chosen `notfound`
                       code.  Backs both PROJ's `tree:`/`blob:` descent and
                       ls-files (replaces proj_descend_inner /
                       lsf_descend_inner).  "." / "./" collapse is the
                       caller's job (done in `proj_descend`).
  - `KEEPTreeListLeaves`  materialise a tree's leaf entries as `(paths, meta)` —
                       newline-sep paths in lex order + parallel 21-byte
                       `{kind, sha[20]}` records.  Feeds `KEEPu8ssDrain` for
                       N-way tree merges (sniff/GET overlap pre-flight uses it).
  - `KEEPTreeDiff`     tree-vs-tree diff → `add`/`del`/`mod` ULOG rows (NULL
                       side = empty tree)
  - Commit-graph traversal lives in `graf/`, not here.

  Range-banner renderers (the "what moved" ULOG status banner GET prints
  on checkout; POST-push and PATCH reuse them for the range they advance —
  all output via `ULOGPrintStatusLine`, no graf, so deterministic
  mid-command):
  - `KEEPEmitCommitLine`     one `post\t?<hashlet8>#<subject>` row for a sha
  - `KEEPEmitTreeDiffFiles`  `<add|del|mod>\t<path>` rows for tree(base)↔tree(tgt)
  - `KEEPEmitCommitsSince`   commit rows for `tip\base` — base reach follows
                             parent **and** foster, so it is ancestor-skip-
                             correct (used by PATCH's absorbed-range banner)

### dog/git/DELT.h — git delta instruction applier + encoder

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

Library `keeplib` (static, see `CMakeLists.txt`):
  KEEP.c KEEP.exe.c MIGRATE.c REFS.c REFADV.c RESOLVE.c WALK.c
  UNPK.c PSTR.c WIRE.c WIRE.rl.c WIRECLI.c RECV.c PROJ.c.
Links: dog, abc-core, ZLIB.  (Git-compat code — GIT.c PKT.c PACK.c
DELT.c ZINF.c IGNO.c plus SHA1 inlines — links in via `dog/git/`.)

Executables: `keeper` (`KEEP.cli.c`), `git-dl` (`git-dl.cli.c`).

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
  - `test/REFKIND.c` `REFSQueryKind` syntactic kind probe — table over
                     canonical trunk/detached/branch shapes + non-canonical
                     rejects (23 cases, no store)
  - `test/RESOLVEURI.c` `REFSResolveURI`/`KEEPResolveURI` funnel — hermetic
                     store (one project, two commits, REFS rows): trunk /
                     detached (sha+hashlet) / branch / nested / relative
                     (`?./sub`, `?..`) / idempotency, plus KEEPResolveURI
                     path + authority passthrough
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
