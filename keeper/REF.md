# keeper refs format

Each keeper project shard has one append-only reflog file holding
*all* of the project's refs — every branch tip, tag and host alias:

    <store>/
        <project>/
            refs                   the project's only reflog

Branches and tags are not directories; they are pure rows in this
single `refs`.  The file is a [dog/ULOG][U] — a plain-text
append-only URI event log.

  [U]: ../dog/ULOG.md

## Row shape

Every row is a standard ULOG row:

    <ron60-ms>\tset\t<from-uri>#?<40-hex-sha>\n

  - **ts**   — RON60 millisecond timestamp, strictly monotonic
    across the file (ULOG enforces; stale appends return
    `ULOGCLOCK`).
  - **verb** — literally `set`.  REFS only emits this one verb today;
    the ULOG verb column is reserved so future revisions (delete,
    move, …) can coexist without rewriting old rows.
  - **uri**  — the (ref-key, sha) pair packed into a single URI so the
    format fits ULOG: the key is everything before `#`, the sha rides
    in the fragment as `?<40-hex>`.  URILexer parses it; REFSLoad
    splits back on `#` to return `{key, val}` pairs.

Typical rows:

    26416FJreE\tset\t?heads/main#?5c9159de87e41cf14ec5f2132afb5a06f35c26b3
    26416FJrfB\tset\t//origin/path?heads/feature#?68aba62e5c4e2f1a07d04a8e3b66c72b4f1a09e2d
    26416FJrCC\tset\t//github#?https://github.com/torvalds/linux.git

Keys in use:

  - `?heads/<name>` / `?tags/<name>` / `?HEAD` — local refs in this
    project shard.  The key starts with `?` because in URI terms only
    the query component is present.
  - `<origin>?heads/<name>` / `<origin>?tags/<name>` — remote-tracking
    views of the same branch seen on a peer.  `<origin>` is whatever
    transport URI the user typed (`ssh://host/path`, `//host/path`,
    `file:///abs`, `//alias`).
  - `//<alias-host>` — host alias row.  The sha-shaped fragment is
    a full URL; `REFSResolve` matches it via host-substring and
    hands back the stored scheme/host/path to the transport layer.
    (Aliases sit in the same file as refs; see
    **Aliases without a sidecar** below.)

## Project scoping

`<project>/refs` records **every** branch tip in the project plus
peer views of those branches.  All branches and tags share this one
file: a wt on `feature` and the trunk's `main` tip both live as rows
here, distinguished by their `?heads/<name>` key, not by directory.

Resolution scans the single `refs`: looking up `?heads/feature`
reads `<project>/refs` and a branch's own rows are authoritative —
there is no dir chain and nothing to walk up.

## Aliases without a sidecar

There is no separate `ALIAS` file and no `alias` verb.  A row whose
key carries an authority (`//github`, `//linux`) plays the alias
role: `REFSResolve` does host-substring matching over each row's
authority in one reverse pass, so `//github?master` matches
`https://github.com/…?heads/master` on the same pass that resolves
`?heads/master` for a local ref.  Most-recent row wins on
ambiguity.

Aliases are just rows in the project's single reflog, alongside
every branch tip and tag.  Dropping a branch tombstones only that
branch's tip rows (see below); alias rows are unaffected.

## What's not in keeper's ref state

**Worktree state** is not in keeper.  The per-wt branch pointer
lives in the wt's `.be` file (tracked by `sniff`, see `sniff/AT.md`);
keeper keeps no per-branch `WT` file.  Keeper's refs carry only
replicated refs (local + remote-attributed) — never "which wt is
where".

## Dropping a branch

Dropping a branch appends a `refs` tombstone for its
`?heads/<name>`; it deletes no objects and no files.  The branch's
objects linger in the single shard until an out-of-band epoch
recompaction (which copies the reachable closure into a fresh
project id).  Resolution stops returning the tombstoned tip, so
peer views of that branch effectively go stale — which is exactly
right once the branch is gone.

## Compaction

`REFSCompact` delegates to `ULOGCompactLatest` — kept rows are
replayed in timestamp order into `refs.tmp` and renamed over the
live file atomically.  Newer rows already shadow older ones during
normal resolution, so compaction only trims history; it never
changes what `REFSResolve` or `REFSLoad` return.  Local-only
operation — no peer negotiation.

## Implementation: thin ULOG glue

`keeper/REFS.c` is a thin wrapper over `dog/ULOG`:

  - **`REFSAppend`**: lex the `from_uri` into a `uri` struct, plant
    `?<sha>` as the fragment, hand to `ULOGAppendAt`.  Clamps the
    timestamp to `max(RONNow(), tail + 1)` so rapid appends within
    the same millisecond still land.  No build-then-reparse string
    round-trip.
  - **`REFSLoad` / `REFSEach`**: call `ULOGeachLatest` with the
    `set` verb filter; the callback peels each row into the caller's
    arena — `URIutf8Feed` with an empty fragment emits the key
    bytes, then the stored fragment is copied verbatim as the val.
  - **`REFSResolve`**: a predicate over `ULOGFindLatest` —
    host-substring match against the row's authority plus
    heads/tags-aware query match, most-recent wins.
  - **`REFSCompact`**: one-liner over `ULOGCompactLatest(set)`.

The 60-bit hashlet dedup in `ULOGeachLatest` means `REFSLoad` no
longer walks the log's text or maintains its own seen-set; it
allocates only the arena bytes it captures into.
