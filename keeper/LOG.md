# keeper pack log

Pack logs are numbered append-only files holding concatenated
*packs*.  They are the sole storage for object bytes; nothing is
ever mutated in place.  The pack format is based on git packfiles.

Logs live in **one flat object pool per project**; a single
`<store>/<project>/` dir holds the whole project's log files plus
its indexes and one `refs` reflog — for *all* local branches, tags
and remote-tracking refs.  There are no per-branch object
subdirectories:

    <store>/
        <project>/              the only object dir
            00001.keeper
            00002.keeper
            00003.keeper
            00001.keeper.idx
            refs                dog/ULOG reflog (see REF.md)

File numbering (`NNNNN`) is **store-wide and sequential**, so a
`file_id` names a log uniquely; the file sits in the single project
shard that wrote it.

## Append-only pack logs

One log file holds **many packs** appended back-to-back.  Small
packs (e.g. a single local commit) MUST be appended to the shard's
current tail log file — never open a new file just to store one
pack.  A new log file is only started when the current one crosses
a size threshold.  A file has the git packfile header but no
trailing checksum.

Pack boundaries inside a log file are discoverable only via the
index (see pack bookmarks below).  The raw bytes carry no
self-synchronising markers (git's limitation, kept for backward
compatibility).

## Intra-pack object order

Within a single pack, objects MUST appear in type order:

    1. commits   (type 1)
    2. trees     (type 2)
    3. blobs     (type 3)
    4. tags      (type 4)

Git relies on this ordering implicitly (`git repack` produces
type-grouped packs for delta-base locality); keeper makes it an
**explicit invariant** — writers should enforce it, readers may
rely on it.

**Current status (2026-04-22):** enforcement in `KEEPPackFeed` is
**live** for canonical packs.  `keep_pack.strict_order` opts in;
`KEEPPackOpen` sets it automatically.  Sniff staging packs
(see `sniff/STAGE.md`) leave it off — they accumulate tree/blob in
DFS order and are repacked canonically on `be post` (commit first,
then trees via staging walk, then blobs).  A handful of legacy
tests that hand-roll non-canonical packs clear the flag explicitly.
`strict_order` checks only **non-decreasing** type, so a pack may
legally start with a tree, blob or tag; a blob-only or tree-only
pack (`KEEPPut` / `KEEPUpdate`) is valid.

Consequences:

  * Objects within a pack follow non-decreasing git type order;
    the pack's first object is **not** required to be a commit.
    Commits are located via the kv64 `type4`-keyed index (range
    over the commit type), NOT by a pack-prefix scan; sync and
    graf find commits through that index, not by inflating bodies.
  * Trees never reference commits; blobs never reference trees or
    commits; delta bases therefore point earlier in the same pack
    or into an earlier pack.  This rules out forward references
    within a pack.
  * Incoming git packfiles already satisfy the ordering when
    produced by stock git; `KEEPImport` copies git's stream
    verbatim (no `Feed`), so the order check does not apply on
    import.

## Stripped git pack framing

Packs are stored with the git packfile header (12 B: `PACK` magic
+ version + object count) and trailer (20 B SHA-1) **stripped**,
except in the beginning of the file. What lands in the log is only
the concatenation of object records (varint header + zlib body,
plus deltas).

This means the stored bytes are not directly a valid git packfile,
but can cheaply be made one. Dog-to-dog sync (see WIRE.md) reframes
on the fly via PSTR (PACK header + concatenated stripped bodies +
fresh SHA-1 trailer).

Object count for reconstruction is read straight from the pack
bookmark's val (`obj_count32 | byte_len32` — see "Pack bookmarks"
below), so per-pack reconstruction (e.g. for the wire encoder in
`WIRE.md` Phase 2) is O(1) — no varint scan, no inflation.  The
git-compat trailing SHA-1 is recomputed on the fly over the
freshly framed bytes when an outgoing pack is sent on the wire.
Per-object 60-bit hashlets in the LSM are untouched.

## Delta dependencies

OFS_DELTA is **pack-local**: the base sits earlier in the same
pack at a known offset.  A pack file can therefore be copied
verbatim between stores (dog-to-dog sync) without rewriting
deltas.

REF_DELTA resolves by hashlet lookup **within the single project
pool**: a base may be any earlier object in the shard.  There is
no dir-chain walk-up.  The only constraint is cross-project: a
REF_DELTA base MUST live in the same project shard, never in
another project, which keeps every shard self-contained and
movable / recompactable in isolation.

## Pack bookmarks

Every pack appended to the log gets one wh128 index entry:

    key = file_id20 | offset40 | type4(PACK)
    val = obj_count32 | byte_len32

`obj_count` is the number of git objects the pack contains;
`byte_len` is the pack's stripped on-disk length, i.e. the number
of bytes of object records that follow the bookmark's `offset` in
`file_id`.  Together they let the wire encoder
(`keeper/PSTR.c`, see `WIRE.md` Phase 2) emit a valid git packfile
in O(1) per pack: sum the counts, write a fresh PACK header,
sendfile each `[offset, offset+byte_len)` range, append a fresh
SHA-1 trailer.  No varint scan, no inflation.

`offset40` is the byte position where the pack's first object
starts within the log file; bookmarks sort by (file_id, offset)
naturally.

The bookmark deliberately does **not** carry a per-pack hashlet.
The retired TLV-based dog↔dog SYNC needed one for dedup
negotiation; the new wire protocol (`WIRE.md`) negotiates over git
sha1s via pkt-line want/have, so the per-pack hash is no longer
useful.  Per-object hashlets in the LSM remain unchanged; the
git-compat trailing SHA-1 is recomputed on the fly when an
outgoing pack is framed for the wire.

Pack byte length is recorded explicitly in the bookmark val (no
need to look at the next bookmark or the file's current length —
both are still consistent with the stored `byte_len`).  No
sentinel is stored in the index itself.

Pack bookmarks share the LSM with object entries; the type tag in
the key's low 4 bits keeps them from colliding (objects use
1..4 for commit/tree/blob/tag; pack bookmarks use a reserved
value outside that range — TBD, propose 15).

## Epoch recompaction (replaces GC / repack)

Keeper never rewrites or GCs packs live.  Cleanup happens out of
band, at the granularity of a **project shard**:

  - **Drop a branch**: write a `refs` tombstone for its
    `?heads/<name>` (see REF.md).  No log file is deleted and no
    objects are removed — the dropped branch's objects simply
    linger in the single shard.  There is no live object deletion
    and no per-branch GC.
  - **Epoch recompaction**: at a major release the project is
    recompacted by copying the reachable closure into a fresh
    project id (e.g. `beagle` → `beagle2`), leaving the now-dead
    objects behind in the old shard.

Readers holding mmaps of an old shard continue to work until they
close.  Because all objects live in one pool, in-shard lookups
never have to route around a missing dir.

Peer watermarks pointing at recompacted `file_id`s become stale —
the affected peers fall back to full sync on next contact via the
standard have/want negotiation (`WIRE.md`).  This is acceptable:
recompaction is user-initiated and watermarks rebuild cheaply.

## Implications for sync

See `keeper/WIRE.md` for the active wire protocol (git pkt-line,
upload-pack / receive-pack compatible).  All of a project's objects
live in one shard, so a fetch resolves its wants/haves within that
single pool and ships the ordered segment list as one freshly-framed
git packfile — there is no per-dir or ancestor-dir fan-out.  Per-pack
`(obj_count, byte_len)` in the bookmark is exactly what the encoder
needs to sum object counts for the new PACK header and to sendfile
each segment in one syscall.  The TLV-based predecessor (`SYNC.md`)
and its per-pack hashlet were removed once `WIRE.md` Phase 10 landed.

## Current code vs. this spec

The flat single-pool model is live.  `KEEPOpenBranch(h, branch, rw)`
opens the one project shard `<store>/<project>/` and registers every
`.keeper` (pack log) and `.keeper.idx` file in it on the
keeper-level `Bkv32 packs` (seqno → fd) and `Bkv32 puppies` (seqno →
fd) registries; `branch` merely selects the *ref context* (which
`?heads/<name>` tip the session reads/advances) — it never names a
directory.  `KEEPLookup` scans all runs in the one pool with no
parent-dir retry.  `KEEPCreateBranch(h, branch)` writes one
`?heads/<name>` ref row (no mkdir).  `KEEPBranchDrop(k, branch)`
appends a REFS tombstone and deletes no objects and no directories
(objects linger for epoch recompaction); it refuses trunk
(`KEEPTRUNK`) and the active branch (`KEEPDIRTY`).  Writes
(`KEEPPackOpen`, `KEEPPackClose`, `KEEPIngestFile`,
`KEEPIngestStream`) and `KEEPCompact` all land in the single shard.
The REF_DELTA visibility check is structural: every entry in
`k->packs` is by construction in the one pool and so visible to any
delta encoded into the shard.
