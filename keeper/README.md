#  keeper — local git object store

Keeper stores git objects in append-only pack logs with LSM-style
indexes, one flat object pool per project.  Packs are git-compatible
and trivially exchangeable with git and git-compatible systems.

Objects are addressed by 60-bit hashlets (15 hex chars of SHA-1
prefix); variable-length prefixes from 4 to 40 chars work for
lookups, matching git's short-hash convention.

Keeper is one of the git dogs and it follows DOG.md conventions,
and integrates with sniff (file tracking), graf (commit graph),
and spot (code search) through whiff/URI conventions.
The URI convention lets you address remotes, refs, and
objects uniformly: `//host/path` to sync, `?refname` to resolve a
ref, `#hashprefix` to cat an object.

##  Usage

```sh
# clone a repo (fetch all refs)
keeper get //localhost/home/user/src/linux

# clone via alias
keeper alias //linux https://localhost/home/user/src/linux
keeper get //linux

# fetch a specific ref
keeper get //linux?tags/v6.0

# resolve a ref to SHA
keeper get .?heads/master

# cat an object by hash prefix
keeper get .#abc1234

# list known refs
keeper refs

# import an existing git packfile
keeper import path/to/pack.pack

# show store stats
keeper status

# verify a commit and all reachable objects
keeper verify .#abc123def456789...
```

Incremental pack writer (C API):

```c
keep_pack p = {};
KEEPPackOpen(&k, &p);
KEEPPackFeed(&k, &p, KEEP_OBJ_BLOB, content, sha_out);  // SHA returned
// use sha_out to build tree entries...
KEEPPackFeed(&k, &p, KEEP_OBJ_TREE, tree_content, tree_sha);
KEEPPackClose(&k, &p);
```

##  Storage layout

A **store** is a directory (`.be/`) holding one or more **project
shards** side-by-side.  Each project shard is a single flat object
pool: one dir holds every `NNNNN.keeper` pack log, every `.idx`
index run, and one `refs` ULOG for *all* of the project's local
branches, tags and remote-tracking refs.  There are no per-branch
object subdirectories.  Cross-project REF_DELTA bases are forbidden
— every project shard is droppable / movable in isolation.

```
<store>/                              the store (`.be/`)
    config                            store-wide TOML config
    <project>/                        project shard (one flat dir)
        refs                          the project's only ref ULOG:
                                      every branch tip + tag + host
                                      alias (`//github → https://…`)
                                      + remote-tracking row, as plain
                                      ref rows
        NNNNN.keeper                  pack logs (append-only)
        NNNNN.keeper.idx              keeper LSM index runs
        NNNNN.graf.idx                graf history index runs
        NNNNN.spot.idx                spot trigram index runs
    <other-project>/                  sibling project shard
        refs
        ...
```

All objects for every branch, tag and remote-tracking ref live in
the one `<project>/` dir.  Branches and tags are not directories;
they are pure ref rows in the single `refs`.  File numbering
(`NNNNN.keeper`, `NNNNN.keeper.idx`, …) is a fresh store-wide
sequence per project shard.

Refs live in the project's single `refs` ULOG.  A branch's tip is
`?heads/<name>` → sha; a branch's refs is authoritative — resolution
does NOT walk up any dir chain (there is no chain).  Host aliases
and project-scoped tags ride in the same `<project>/refs`.

Worktrees are separate checkouts on disk.  Two flavors:

  * **Colocated** — `<wt>/.be/` is the store directory; the store
    holds one project shard alongside the wt's files.  The wt
    command log lives at `<wt>/.be/wtlog`; row 0's `repo` URI
    pins the project (`file:<wt>/.be/<project>`).
  * **Central** ("`$HOME/.be/`" pattern) — `<wt>/.be` is a regular
    *file* (not a directory) carrying a wtlog whose row 0 names
    a project under a remote store:

```
<wt>/.be        contents: file:/abs/store/.be/<project>
                followed by `get`/`post`/... rows for branch state
```

The branch the wt is on is NOT in the anchor URI — it is the
latest `get`/`post` row's `?branch` in the wtlog.  Switching
branches appends a new row, never rewrites the anchor.

The reverse pointer (which wts are pinned to which branches) is
derivable by walking known wtlogs; keeper does not maintain a
per-branch `WT` file.

##  Pack log files

Pack logs are append-only git-encoded object files.  They are close
to the git packfile format but NOT valid git packfiles:

  - The first batch (initial clone/fetch) starts with a standard
    PACK header (magic + version + count), received verbatim from
    `git-upload-pack`.
  - Subsequent fetches and local writes (`KEEPPackFeed`) **append**
    new objects to the same log file and update the count.  There
    is no trailing SHA-1 checksum.
  - OFS_DELTA references are **pack-local**: the base sits earlier
    in the same pack at a stable offset.  Whole packs can be copied
    verbatim between stores without rewriting.
  - REF_DELTA references resolve by hash through the LSM index
    within the single project pool (see "Delta dependencies").

Objects are never moved or rewritten; offsets within a log are
stable.  The log is written via `FILEBook` (mmap'd, growable) and
read via `FILEMapRO`.

```
PACK v2 N       12 bytes: magic, version, count (first batch only)
obj 0           varint(type+size) + zlib(content|delta)
obj 1
...
obj N-1         end of first batch
obj N           appended by next fetch or KEEPPackFeed
obj N+1
...             (no trailing checksum)
```

##  Index entries (kv64)

Each index entry is 16 bytes: u64 key + u64 val.

```
key = hashlet60 | obj_type4
val = offset40  | file_id20 | flags4

obj_type   meaning
────────   ───────
0001       commit
0010       tree
0011       blob
0100       tag
```

A **hashlet** is the first 60 bits of the SHA-1 in big-endian order
(first byte on top).  The low 4 bits carry the git object type, so
entries sort by hashlet first, type second.  Lookups by hash prefix
span all types via range query.

The **val** uses the wh64 layout: `offset40` is the byte position
within the log file, `file_id20` is the store-wide sequential number
of the pack log (the `NNNNN` of `NNNNN.keeper`), `flags4` is
reserved.  `file_id`s are unique across the whole store: every pack
log has exactly one filename like `NNNNN.keeper` physically sitting
in the single project shard.  All index runs and pack logs share
that one dir, so the `file_id` resolves to a file with no separate
`file_id → dir` map and no per-dir search.

##  Index management

Index runs are per-project sorted kv64 files in the single shard:

-   **Write**: sort new entries, flush to `NNNNN.idx` in the shard
-   **Read**: mmap all runs in the shard, binary search each
-   **Compact**: merge runs when LSM invariant violated (within shard)
-   **Lookup**: range query
    `[hashlet_prefix << 4, hashlet_prefix << 4 | 0xf]`;
    over every run in the one pool — a miss is final, there is no
    parent-dir walk-up

##  Delta dependencies

All of a project's objects share one flat pool; a delta base may be
any earlier object in that pool:

  - **OFS_DELTA** — always pack-local; the base sits earlier in the
    same pack.
  - **REF_DELTA** — resolved by hashlet lookup within the single
    project pool, against any earlier object in the shard.  There is
    no dir walk-up.  The only constraint is cross-project: a base
    MUST live in the same project shard, never in another project.

This keeps every project shard self-contained, so it copies or
recompacts in isolation.  Dropping a branch is a `refs` tombstone,
not an object delete; objects linger until an out-of-band epoch
recompaction copies the reachable closure into a fresh project id
(e.g. `beagle` → `beagle2`).  Keeper never rewrites or GCs existing
packs — this replaces git-style repack.

##  Object resolution

1.  Compute 60-bit hashlet from hex prefix (zero-pad short prefixes).
2.  Range query the LSM index across the single project pool.
3.  A miss is final — there is no parent-dir retry.
4.  First match gives `(file_id, offset)` in the one shard.
5.  Read pack object header at that offset in the shard's log.
6.  Base type (commit/tree/blob/tag): inflate directly.
7.  OFS_DELTA: base is at `offset - delta` within the same pack.
8.  REF_DELTA: look up base by hashlet (same range query as 2).
9.  Chase delta chain, apply `DELTApply` bottom-up.

##  refs and aliases

`<project>/refs` is the project's single append-only reflog holding
every branch tip, tag and host alias as rows.  The file is a
`dog/ULOG` — plain-text rows of the form

```
<ron60-ms>\tset\t<from-uri>#?<40-hex-sha>\n
```

The verb is always `set`; the key/val pair is packed into a single
URI so it fits ULOG's `(ts, verb, uri)` shape (key before `#`, sha
in the fragment).  `REFSLoad` splits back on `#` to return
`{key, val}` pairs; ULOG enforces strict monotonicity of the
timestamp column.  See `REF.md` for the full format.

A local tip uses a query-only key:

```
26416FJreE\tset\t?heads/main#?5c9159de87e41cf14ec5f2132afb5a06f35c26b3
```

Remote-tracking of the same branch uses a fully-qualified origin:

```
26416FJrfB\tset\t//origin/path?heads/feature#?68aba62e5…
```

Host aliases ride in the same file — no sidecar `ALIAS` file.
The fragment is a full URL rather than a sha; `REFSResolve` matches
by host-substring in one reverse pass and hands the stored
scheme/host/path back to the transport layer:

```
26416FJrCC\tset\t//github#?https://github.com/torvalds/linux.git
```

A branch's tip rows in `refs` are authoritative — resolution does
NOT walk up any dir chain (there is no chain).  Aliases and tags
ride in the same single `<project>/refs`.

Which commit the wt is currently checked out on is tracked by
`sniff` (see `sniff/AT.md`), not by keeper.

##  Dependencies

-   `abc/KV.h` — kv64 type and sorted-run operations
-   `abc/FILE.h` — FILEBook (growable mmap), file I/O
-   `abc/URI.h` — URI parsing for CLI
-   `abc/RON.h` — ron60 timestamps, base64 sequence numbers
-   `keeper/PACK.h` — packfile header/object parsing
-   `keeper/DELT.h` — delta apply (and encode)
-   `keeper/ZINF.h` — zlib inflate/deflate
-   `keeper/SHA1.h` — SHA-1 hashing
-   `keeper/GIT.h` — commit/tree/blob parsing
-   `keeper/REFS.h` — ULOG-backed URI reflog (see `REF.md`)
-   `dog/ULOG.h` — append-only URI event log (refs file format)
-   `dog/WHIFF.h` — wh64 tagged-word packing (val format)
