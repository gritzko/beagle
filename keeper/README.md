#  keeper — local git object store

Keeper stores git objects in append-only pack logs with LSM-style
indexes, sharded by branch.  Packs are git-compatible and trivially
exchangeable with git and git-compatible systems.

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

A **store** is a directory containing trunk plus nested branch
subdirectories, each path-like to mirror `?refname` URIs:

```
<store>/                       trunk (aliases heads/main,
                               heads/master, heads/trunk → "")
    refs                       trunk reflog (dog/ULOG; carries both
                               local tips and host aliases)
    WT                         abspath of worktree on trunk (if any)
    NNNNN.keeper               trunk pack logs (append-only)
    NNNNN.idx                  trunk LSM index runs
    NNNNN.spot                 other dogs' per-branch artefacts
    feature/                   `?heads/feature`
        refs
        WT
        NNNNN.keeper
        NNNNN.idx
    heads/fix/                 `?heads/fix` (explicit path also
                               accepted; `heads/` prefix is optional)
    tags/v0.0.1/               `?tags/v0.0.1`
```

Branch directories nest freely: `feature/fix1/` is a valid
sub-branch of `feature/`.  File numbering (`NNNNN.keeper`,
`NNNNN.idx`, …) is a fresh sequence per directory.

Worktrees are separate checkouts on disk; each wt has a `.be`
*file* (not a directory) naming the store plus the branch it is on:

```
<wt>/.be        contents: /abs/store?heads/feature
```

The branch directory holds the reverse pointer in `WT`, so each
branch is linked to at most one wt.  A store-co-located wt
(traditional layout) uses `<repo>/.be/` as the store dir and keeps
its working files next to it; external wts use the `.be` file.

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
  - REF_DELTA references resolve by hash through the LSM index and
    follow the branch hierarchy (see "Delta-dependency DAG").

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
in whichever branch dir the pack was written to.  The branch dir
that holds a given `file_id` is recovered from whichever index run
returned the hit (index entries only reference packs in the same
dir), so no separate `file_id → dir` map is needed.

##  Index management

Index runs are per-branch-directory sorted kv64 files:

-   **Write**: sort new entries, flush to `NNNNN.idx` in the same dir
-   **Read**: mmap all runs in the dir, binary search each
-   **Compact**: merge runs when LSM invariant violated (within dir)
-   **Lookup**: range query
    `[hashlet_prefix << 4, hashlet_prefix << 4 | 0xf]`;
    on miss, walk parent dir, … up to the store root

##  Delta-dependency DAG

Branch directories form a DAG rooted at the store root.  Objects
may reference bases only within the same dir or in an ancestor dir:

  - **OFS_DELTA** — always pack-local; unaffected by branch layout.
  - **REF_DELTA** — resolved by hashlet lookup, which walks up the
    dir chain.  Writers MUST NOT delta a new object against a base
    that lives in a sibling or descendant dir; **materialize** the
    base (emit it raw into the target dir's pack) instead.

The invariant makes "drop a branch dir" safe by construction: a
leaf dir can be removed wholesale (pack logs, indexes, reflog, WT)
without invalidating any surviving REF_DELTA.  An interior dir can
only be dropped once its descendants have materialized every
reference into it — a precondition check, not an automatic
rewrite.  This replaces git-style repack; keeper never rewrites
existing packs.

##  Object resolution

1.  Compute 60-bit hashlet from hex prefix (zero-pad short prefixes).
2.  Range query the LSM index in the current branch dir.
3.  On miss, walk up to the parent dir and retry; repeat to root.
4.  First match gives `(dir, file_id, offset)`.
5.  Read pack object header at that offset in that dir's log.
6.  Base type (commit/tree/blob/tag): inflate directly.
7.  OFS_DELTA: base is at `offset - delta` within the same pack.
8.  REF_DELTA: look up base by hashlet (same walk as 2-3).
9.  Chase delta chain, apply `DELTApply` bottom-up.

##  refs and aliases

`<branch-dir>/refs` is an append-only reflog scoped to that one
branch.  The file is a `dog/ULOG` — plain-text rows of the form

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

A branch's `refs` is authoritative for its own tips — resolution
does NOT walk up the dir chain.  Aliases that must survive a
branch-dir drop belong in the root's `refs`.

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
