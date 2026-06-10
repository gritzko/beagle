# ULOG — append-only URI event log

A generic, reusable building block for "what happened, when, where".
Each row is a `(timestamp, verb, URI)` triple; timestamps are strictly
monotonic; the file is append-only plain text.

Used today by `sniff` for its attribution log (worktree checkouts,
stages, commits).  Fits any event stream where monotonic ordering plus
a CLI-shaped `(verb, URI)` payload is enough, e.g. a graf activity
log, a build provenance trail, or a crash-safe queue of pending syncs.

## Format

```
<ron60-ms>\t<verb>\t<uri>\n
<ron60-ms>\t<verb>\t<uri>\n
...
```

- **Timestamp** is a RON60 millisecond stamp (`abc/RON.h`), encoded in
  the RON base64 alphabet — variable width, sorts lexicographically
  the same way it sorts numerically.
- **Verb** is another RON60 (≤10 base64 chars) naming the operation
  that produced the row — `get`, `put`, `post`, `delete`, `patch`,
  `sync`, etc.  Stored as text, compared as `u64` (one cache line,
  zero string compares in hot paths).
- **Separator** is one or more SP/TAB bytes.  RON base64 and URI byte
  alphabets both exclude whitespace, so the split is unambiguous.
  Writers emit a single TAB (canonical); readers tolerate arbitrary
  whitespace runs.
- **URI** is anything `abc/URI.h:URILexer` accepts — `scheme://auth/path?query#frag`,
  bare `//host/path`, relative `?query`, etc.  It takes the rest of
  the line; URIs cannot contain SP, TAB, or LF per RFC 3986.
- **Row terminator** is a single LF (`\n`).  No CR, no continuation.
- A blank line is tolerated on read (treated as padding) but never
  emitted on write.

The format is deliberately plain text: `cat`, `grep`, `tail -f`, and
`awk -F'\t'` all work as expected for debugging and post-hoc analysis.

### Monotonicity

For all rows `r_i` and `r_{i+1}`: `r_{i+1}.ts > r_i.ts`.

`ULOGAppend` refuses to emit a non-monotonic row with `ULOGCLOCK`.
`ULOGOpen` verifies the invariant by scanning the file on load, and
bails with `ULOGCLOCK` if it finds two adjacent rows with
`ts_{i+1} <= ts_i` (clock skew, out-of-order append, or corruption).
There is no automatic repair — the caller decides what to do with a
file whose log violates monotonicity (typically: error out, require
manual inspection).

A 1-millisecond resolution is enough granularity that accidental
duplicates within a single monotonic `RONNow()` clock source are
astronomically unlikely; the check is mostly a guard against deliberate
clock jumps (NTP steps, VM time travel, `date -s`).

## Layout on disk and in RAM

- **On disk**: one text file, `FILEBook`'d.  A book reserves a large
  VA range (1 GiB by default) but only maps the file's actual length;
  appends extend the file and the mapping without relocating the base
  pointer.  `FILETrimBook` on close writes the real length back so the
  next open sees no zero-padded tail.
- **Sidecar index**: hidden sibling at `<dir>/.<base>.idx`, a packed
  array of `wh128` entries — `{u64 ts-key, u64 (offset40, verbHash20,
  type4)}`.  One entry per row plus a tail sentinel that records the
  log's `(mtime, byte size)` as of the last close.  On open, the sidecar
  is trusted as-is (O(1) — no scan, no row re-parse) only when ALL of:
  the sentinel's mtime matches the log's current `fstat`, the sentinel's
  size matches, AND the index's LAST real row actually spans to the
  log's content end (`ulog_idx_spans_log`).  The row-coverage check
  guards against a "false-fresh" sidecar (DIS-033): a clone-copied or
  sentinel-only-refreshed sidecar can carry a matching `(mtime, size)`
  while its row entries describe just an older, shorter prefix — reading
  it would silently drop the tail rows.  Any failing condition falls
  through to a linear rebuild (and the rebuilt index is written back to
  the sidecar).
- **In RAM**: the sidecar mapping itself.  Reads index into it directly.

The mmap-backed log text is the ground truth; the sidecar is a derived
cache, recoverable any time by rescanning the log.  `ULOGRow` returns a
`uri` whose component slices point into the log mmap, so there is no
per-row allocation.

## Operations

| Call | Cost | What |
|------|------|------|
| `ULOGOpen` | O(1) fresh / O(N) stale | sidecar mtime+size match → map as-is; mismatch → linear rescan + sidecar rewrite |
| `ULOGClose` | O(1) | trim book, refresh sentinel (mtime+size), flush sidecar, unmap |
| `ULOGAppend` / `ULOGAppendAt` | O(1) amortised | emit row, push index entry |
| `ULOGCount` | O(1) | index size |
| `ULOGRow(i)` | O(1) | index lookup + parse one line |
| `ULOGHead` / `ULOGTail` | O(1) | wrappers over `ULOGRow` |
| `ULOGSeek(ts)` | O(log N) | lower_bound on timestamp column |
| `ULOGFind(ts)` | O(log N) | exact match or `ULOGNONE` |
| `ULOGHas(ts)` | O(log N) | membership — the "is this mtime one of our stamps?" check |
| `ULOGFindVerb(verb)` | O(hits) | reverse scan for the latest row with that verb |
| `ULOGFindLatest(pred)` | O(hits) | reverse scan until predicate holds |
| `ULOGeachLatest(verb)`  | O(N + K²) | iterate unique (verb, URI-minus-fragment) keys, latest per key, reverse-chron order |
| `ULOGCompactLatest(verb)` | O(N + K²) | rewrite via `<path>.tmp` + rename, keeping latest per key |
| `ULOGTruncate(keep_n)` | O(1) | rewind book + shorten index; no rewrite |
| `ULOGu8sFeed` / `ULOGu8sDrain` | O(1) per row | streaming codec for pipe / tail-f consumers |

### Latest-per-key dedup

`ULOGeachLatest` and `ULOGCompactLatest` treat the URI bytes up to
the first `#` as a *key*, and the fragment as its *value*.  Two rows
with the same non-fragment URI shadow each other; the latest wins.
The dedup key is `rapidhash(verb ⊕ key-bytes)` — verbs are part of
the key, so `get ?heads/main` and `set ?heads/main` are distinct.

Collision risk at 64-bit hash: ≈ N²/2⁶⁵ per unique-key pair.  For a
1M-key log that's ~10⁻⁸; adequate for reflog-scale workloads.  A
future caller that needs byte-exact dedup can layer a secondary
compare on top or swap in a proper hash table.

The "keep-set" used during the walk is a linear-probe `Bu64`, so
cost is O(N + K²) where K is the number of unique keys.  For
realistic reflog sizes (K ≤ a few thousand) the K² term is a rounding
error; larger logs should compact more often rather than relying on
this primitive at scale.

## Failure modes

| Code | Meaning |
|------|---------|
| `ULOGFAIL` | Generic I/O or API misuse |
| `ULOGNONE` | Requested row / timestamp does not exist |
| `ULOGCLOCK` | Monotonicity violated (on append or during scan) |
| `ULOGBADFMT` | Row parse error (missing `\t`, malformed RON timestamp) |
| `ULOGTORN` | Torn / partially-zeroed log: a NUL byte sits before real content in a file that is not genuinely empty (interrupted / ENOSPC / SIGKILL write, or page-cache loss).  Open refuses rather than present an empty log — see below. |

There is no partial-write recovery for a clean *trailing* partial
row: if the process dies mid-append, the trailing partial row (no
`\n`) is simply ignored by the scanner — it parses through
newline-terminated rows only.  The next append writes after whatever
the previous last `\n` was, overwriting the partial bytes.

**Torn-log refusal (ULOG-001).**  A NUL byte that appears *before* the
file's real on-disk content end (e.g. byte 0 clobbered to NUL by a
torn / interrupted / page-cache-lost write) is NOT a clean zero-pad
tail.  The scanner used to stop at it and report zero rows; the next
RW close then `FILETrimBook`'d the in-memory data length (0) back to
disk, `ftruncate`-ing the surviving history to **0 bytes** while the
companion object packs stayed intact — the observed `refs`/`wtlog`
zeroing.  The scanner now returns `ULOGTORN` when it stops at a NUL
with non-NUL bytes still ahead inside the content region; the open
errors out, the file's original size is restored, and the live bytes
are left exactly as found for out-of-band recovery — never zeroed.
A genuinely all-NUL or 0-byte file is still treated as empty.

## Non-goals

- **No arbitrary payloads**.  The payload is a URI.  If you want raw
  bytes, wrap them in a `data:` URI or pick a different module.
- **No compaction heuristics built in**.  `ULOGTruncate` is the primitive;
  callers decide when to run it and how to preserve meaning (e.g.
  sniff's rule: keep the tail row and any row whose timestamp any live
  file mtime still matches).
- **No concurrent writers**.  Single-writer by construction; use an
  external lock (`flock`, keeper's shard lock, etc.) if multiple
  processes may append.
- **Coarse + tail-coverage freshness**.  The freshness check is
  whole-file `(mtime, size)` plus a single tail-row span check
  (`ulog_idx_spans_log`): the last indexed row must reach the log's
  content end.  There is still no full per-entry validation, but the
  tail check catches the "false-fresh" sidecar (DIS-033) whose
  `(mtime, size)` match the log while its rows cover only a shorter
  prefix — exactly the shape a clone-copied or sentinel-only-refreshed
  sidecar can take.  A torn sidecar whose sentinel matches by accident
  is likewise caught when its rows do not span the log.
