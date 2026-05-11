# keeper refs format

Each keeper branch dir has one append-only reflog file:

    <store>/
        refs                       trunk reflog
        feature/
            refs                   feature branch reflog
        tags/v1.0/
            refs                   tag history

The file is a [dog/ULOG][U] — a plain-text append-only URI event log.

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
    branch dir.  The key starts with `?` because in URI terms only
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

## Per-branch scoping

`<branch-dir>/refs` records **this one branch's** tip plus peer
views of the same branch.  Cross-branch entries do not belong
here: if a wt on `feature` learns origin's `main` tip, that row
lives in the trunk's `refs` (the dir that owns `main`), not in
`feature/`.  Placement rule: the entry's branch-part must match
the owning dir.

Resolution is scoped to the dir: looking up `?heads/feature` reads
`feature/refs` directly and does **not** walk up the dir chain.

## Aliases without a sidecar

There is no separate `ALIAS` file and no `alias` verb.  A row whose
key carries an authority (`//github`, `//linux`) plays the alias
role: `REFSResolve` does host-substring matching over each row's
authority in one reverse pass, so `//github?master` matches
`https://github.com/…?heads/master` on the same pass that resolves
`?heads/master` for a local ref.  Most-recent row wins on
ambiguity.

Because aliases are just rows in the dir's reflog, dropping a dir
drops its alias rows too — exactly like its local tips.  Cross-host
aliases that must survive a branch-dir drop belong in the root's
`refs`.

## What's not in keeper's ref state

**Worktree state** is not in keeper.  The per-wt branch pointer
lives in the wt's `.be` file; the reverse pointer lives in the
branch dir's `WT` file (see `sniff/AT.md`).  Keeper's refs carry
only replicated refs (local + remote-attributed for the same
branch) — never "which wt is where".

## Drop-a-dir and reflogs

Squash/rebase/drop of a branch dir removes its `refs` together
with its packs.  Peer views of that branch held in that dir's
`refs` vanish too, which is exactly right: the dropped commits
are gone, so the memory of where peers had them no longer
matters.

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
