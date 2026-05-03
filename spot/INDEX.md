# spot/ — code search, replace, grep

Spot is a producer of hunks for search results. Display lives in
[bro/](../bro/INDEX.md). Diff and merge live in
[graf/](../graf/INDEX.md). When stdout is a tty, spot forks bro as
a pager and writes TLV hunks (see [dog/HUNK.h](../dog/HUNK.h)) to
the pipe. Otherwise it writes plain ASCII directly to stdout via
`HUNKu8sFeedText`.

## Headers

| Header | Purpose |
|--------|---------|
| CAPO.h | Main API: index, search, grep |
| CAPOi.h | Internal: shared helpers, CAPOFindExt macro, HIT u64cs template |
| LESS.h | Producer staging: scratch arena, `spot_out_fd`, `spot_emit`, `LESSHunkEmit` |
| SPOT.h | Structural pattern matching: tokenize, init, next, replace |

## Source files

| File | Purpose |
|------|---------|
| CAPO.c | Index management, SPOT search, hunk-building helpers |
| GREP.c | Substring grep (CAPOGrep), regex grep (CAPOPcreGrep) |
| CAPO.cli.c | CLI entry point: arg parsing, fork bro, pick `spot_emit` |
| LESS.c | Staging arena + `LESSHunkEmit` (serialize via `spot_emit` → `spot_out_fd`) |
| SPOT.c | SPOT pattern matching engine, needle flattening, replacement |

## Key functions (CAPO.h)

| Function | Purpose |
|----------|---------|
| `CAPOSpot` | Structural search (and replace) across repo |
| `CAPOGrep` | Substring grep with syntax-highlighted context (GREP.c) |
| `CAPOPcreGrep` | Regex grep via Thompson NFA + trigram filtering (GREP.c) |
| `CAPOCompact` / `CAPOCompactAll` | Compact LSM index runs |
| `CAPOResolveDir` | Resolve `<workspace>/.dogs/spot` dir |
| `CAPOIndexBlob` | Tokenize a streaming blob, emit `spot64` postings keyed by precomputed `fn_rap40` |
| `CAPOIndexFile` | Search-time wrapper: hash basename, delegate to `CAPOIndexBlob` |
| `CAPOFnRap40` | `RAPHash(basename) & ((1<<40)-1)` — the 40-bit posting key |

Ingestion is driven by keeper's UNPK emit hook (`SPOTUpdate` per
resolved object), so a `keeper get` / `sniff get` indexes every blob
inline.  `spot get` is a no-op kept for orchestration uniformity.
The dispatch is order-tolerant beyond a single guarantee:

- `COMMIT` — ignored.
- `TREE` — for each `(name, child_sha)` entry whose basename has a
  known tokenizer ext, stamp `blob_to_fn[hashlet60(child_sha)] =
  (CAPOFnRap40(name) << 24) | ext_off`.  Subtrees and untokenizable
  blobs are skipped — no chain, no parent state.
- `BLOB` — look up `(fn_rap, ext_off)`; on hit, tokenize inline and
  emit postings via `CAPOIndexBlob`.  Miss = no tokenizable basename
  in any tree we've seen → silent skip.

Pack producers (git, sniff) emit trees before blobs, so the lookup
always hits.  No buffering, no deferred-tree replay, no commit-root
seeding.

Historic search (`spot … ?ref`) goes through `CAPOScanRef`: resolves
the ref via keeper, walks the tree, pulls each blob-of-matching-ext,
and runs the usual grep/pcre/snippet callbacks on the blob content.
`spot --replace` is refused when `?ref` is set (no on-disk file).

Worktree search uses the basename-RAP filter for speed: paths whose
basename's `fn_rap40` carries no needle-trigram entry are skipped.
Two files with the same basename in different directories share one
posting bucket; both pass the filter and the worktree scan rescans
each.  Strictly-untracked brand-new files with novel trigrams are
still candidates as long as their basename appears in any indexed
blob — sniff-changed-file enumeration could bypass the filter
explicitly; not currently wired.

## Key functions (SPOT.h)

| Function | Purpose |
|----------|---------|
| `SPOTTokenize` | Tokenize source into packed u32 buffer via tok/ |
| `SPOTInit` | Initialize pattern matcher (tokenizes needle) |
| `SPOTNext` | Find next structural match (OK or SPOTEND) |
| `SPOTReplace` | Apply replacement to all matches in one file |

## Internal helpers (CAPO.c, static)

| Function | Purpose |
|----------|---------|
| `CAPOFindFunc` | Walk backward to find enclosing function name |
| `CAPOGrepCtx` | Compute context line range around a byte position |

## Index format

Index lives in `.dogs/spot/*.spot.idx` (under the workspace's
`.dogs/`).  Each entry is one `spot64` (`u64`) with the layout

```
[ id:20 | type:4 | fn_rap:40 ]   (high → low, natural u64 sort)
```

| `type` | `id` payload |
|--------|---------------|
| `SPOT64_TRI` (0) | 18-bit packed RON64 trigram (2 spare bits) |
| `SPOT64_MEN` (1) | `RAPHash(symbol_name) & 0xFFFFF`     — `S`/`C` tags |
| `SPOT64_DEF` (2) | `RAPHash(symbol_name) & 0xFFFFF`     — `N` tag |
| 3..15 | reserved |

`fn_rap` is `RAPHash(basename) & ((1<<40)-1)` — the leaf basename
only, no path.  Sorting clusters by `id` first (so seek-by-trigram
is a contiguous `1<<40` range), then `type`, then `fn_rap`.  Files
are sorted MSET runs, compacted via LSM-style merging.
