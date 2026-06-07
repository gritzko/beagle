#ifndef SNIFF_AT_H
#define SNIFF_AT_H

//  AT — sniff's per-worktree state log, persisted at `<wt>/.be/wtlog`
//  as a ULOG (see dog/ULOG.md).  Each row records a worktree-changing
//  op: checkout (`get`), stage (`put` / `delete`), commit (`post`),
//  patch (`patch`).  The row's timestamp is the ms at which the op
//  ran; sniff stamps every file it writes with that timestamp via
//  `futimens`, so file mtime ∈ log-stamp-set means "clean, attributed
//  to the matching row's URI".
//
//  URI schema for rows (new ULOG-only model):
//    get    `file:///abs/path/.be/`      (wt → store anchor; row 0;
//                                          doubles as the "last get"
//                                          baseline — carries no #sha)
//    get    `//origin/path?heads/X#<sha>`  (checkout from remote)
//    get    `?heads/X#<sha>`               (local checkout by ref)
//    get    `?#<sha>`                      (trunk-state: trunk @ sha)
//    get    `?<sha>`                       (detached checkout: sha in
//                                           QUERY, fragment empty)
//    post   `?heads/X#<sha>`               (local commit on branch)
//    post   `?#<sha>`                      (trunk-state commit on trunk)
//    --     detached (`?<sha>`) → POST/PATCH refuse (DIS-009)
//    patch  `?heads/X#<ours>,<theirs>,...` (N-hash merge URI, graf-readable)
//    put    `<path>`                       (explicit stage of one path)
//    delete `<path>`                       (explicit stage of one removal)
//    mod    `<path>`                       (daemon-observed modification;
//                                           advisory hint for POST's change-set)
//
//  Row-0 invariant: every non-empty `.be/wtlog` ULOG has an anchor
//  row at row 0 naming the store (the directory whose `.be/` subdir
//  holds the keeper pack-log).  Its verb is `get` (the get-
//  unification; legacy stores wrote `repo` — readers accept both).
//  Colocated default: the wt's own `.be/`.  Secondary worktrees
//  sharing a primary's store record that primary's URI here.  The
//  anchor `get` carries no `#sha`, so it self-excludes from cur-tip
//  resolution while serving as the "last get" baseline floor.
//
//  Baseline rule: the worktree's current baseline tree URI is the URI
//  of the most recent `get`, `post`, or `patch` row.  Hash count
//  selects the tree backend — one hash → keeper (plain commit tree),
//  two or more → graf (merged tree).  `patch` appends one hash per
//  merge; `post` collapses to a single new commit sha.
//
//  Stamp-set rule: each `get`, `post`, `patch` row stamps every file
//  it touched with the row's timestamp via `futimens`.  A file is
//  "clean" iff its mtime equals one of those stamps.  `put` / `delete`
//  rows are pure intent and do not stamp anything.

#include <time.h>

#include "abc/BUF.h"
#include "abc/INT.h"
#include "abc/PATH.h"
#include "abc/RON.h"
#include "abc/URI.h"
#include "dog/ULOG.h"
#include "dog/WHIFF.h"

con ok64 SNIFFNONE = 0x1c5d23cf5d85ce;

//  Standalone RO peek at `<wt>/.be/wtlog` — does NOT touch the SNIFF
//  singleton, does NOT open keeper.  Composes a URI carrying the
//  worktree's full anchor:
//    path     = repo root (parent of `.be/`, from the row-0 `repo`
//               URI; `.be/` segment is stripped).
//    query    = current be-side branch path (empty == trunk), from
//               the latest `get`/`post`/`patch` row.
//    fragment = current 40-hex commit sha, same row.
//  The text `<root>?<branch>#<sha>` is fed into `out` and is then both
//  URILexer-parseable and ready to forward as `--at <out>` to a
//  sub-dog.  Returns SNIFFNONE on a missing file, no `repo` row, or
//  no row carrying a 40-hex sha.
ok64 SNIFFAtTailOf(u8cs wt, u8bp out);

//  Append one row to the current sniff log using `RONNow()`.
ok64 SNIFFAtAppend(ron60 verb, uricp u);

//  Populate an anchor URI's QUERY `/<title>[/<branch>]` and FRAGMENT
//  `<hash>` for the sha-bearing row-0 shape (DIS-001).  `qbuf` backs
//  the query bytes and must outlive `urow`'s serialization; `hash` is
//  referenced in place.  Empty title → no query (bare anchor); empty
//  hash → no fragment; empty branch → the project's trunk.
ok64 SNIFFAtAnchorRef(uri *urow, u8bp qbuf, u8cs title, u8cs branch,
                      u8cs hash);

//  Write a one-row wt anchor file: composes the ULOG `get` row
//  `<RONNow()>\tget\tfile:<repo_path>[?/<title>/<branch>][#<hash>]\n`
//  and writes it to `anchor_path`.  Used by both layouts:
//    * primary wt → `anchor_path = <wt>/.be/wtlog` (file inside
//      the `.be/` directory)
//    * secondary wt → `anchor_path = <wt>/.be` (`.be` IS the file)
//  When (title, branch, hash) are non-empty the row carries the
//  sha-bearing `?/<title>/<branch>#<hash>` (DIS-001); pass empty for
//  the bare tip-less anchor.  Caller has already decided the layout +
//  filesystem position.  Creates the file (truncating any prior
//  content).
ok64 SNIFFWtRepoAnchor(u8cs anchor_path, u8cs repo_path, u8cs title,
                       u8cs branch, u8cs hash);

//  Like SNIFFAtAppend but takes an explicit timestamp.  Used by verbs
//  that need to know the stamp up-front so it can be applied to
//  fresh files via `futimens` before the row is committed.
ok64 SNIFFAtAppendAt(ron60 ts, ron60 verb, uricp u);

//  `mtime ∈ log-stamp-set?` — the attribution test.  True when some
//  row stamped a file with exactly this timestamp.  Clean files on
//  disk pass this check; user edits do not.
b8   SNIFFAtKnown(ron60 mtime);

// --- New-model helpers (used by the rewritten GET/PUT/DEL/POST/PATCH) ---

//  Encode a u8 slice holding a verb name ("get", "post", ...) into its
//  ron60 representation.  Consuming.  Caller normally caches the result.
fun ron60 SNIFFAtVerbOf(u8cs name) {
    a_dup(u8c, d, name);
    ron60 v = 0;
    RONutf8sDrain(&v, d);
    return v;
}

//  Cached verb constants.  First call initialises; subsequent calls
//  are branch-predicted loads.  Prefer these over re-encoding.
ron60 SNIFFAtVerbRepo  (void);
ron60 SNIFFAtVerbGet   (void);
ron60 SNIFFAtVerbPost  (void);
ron60 SNIFFAtVerbPatch (void);
ron60 SNIFFAtVerbPut   (void);
ron60 SNIFFAtVerbDelete(void);
ron60 SNIFFAtVerbMod   (void);

//  Read row 0 — the anchor (verb `get`, legacy `repo`).  On OK, `u_out` is parsed via
//  URILexer (slices point into the mmap, stable until ULOGClose);
//  its path component is the on-disk path of the store's `.be/`
//  directory.  ULOGNONE on an empty log.  Returns SNIFFFAIL if the
//  first row exists but its verb is not `repo`.
ok64 SNIFFAtRepo(urip u_out);

//  Baseline tree URI — the most recent row whose verb is `get`, `post`,
//  or `patch`.  On OK, `*ts_out` is the row's timestamp, `*verb_out`
//  is its verb, and `u_out` is parsed via URILexer (components point
//  into the mmap, valid until ULOGClose / ULOGTruncate — same contract
//  as ULOGRow).  ULOGNONE on an empty log or one with only put/delete
//  rows (shouldn't happen in practice — a bare log has no baseline).
ok64 SNIFFAtBaseline(ron60 *ts_out, ron60 *verb_out, urip u_out);

//  "Current tip" — like `SNIFFAtBaseline` but skips `patch` rows and
//  returns the most recent `get` or `post`.  Use this when you want
//  the wt's anchor commit (parent for the next post, tree to diff
//  against, branch label) rather than the literal latest baseline
//  row.  Patch rows now record the *absorbed* commit's sha in their
//  fragment (not the wt's tip); their `theirs` chain is recovered
//  via `SNIFFAtPatchChain`.  Returns ULOGNONE when no get/post row
//  is present.
ok64 SNIFFAtCurTip(ron60 *ts_out, ron60 *verb_out, urip u_out);

//  Resolve the baseline tree sha from sniff's ULOG.  Chains the four
//  steps every commit-time / classify-time helper used to open-code:
//  pick the baseline row (cur_tip_only ? SNIFFAtCurTip : SNIFFAtBaseline),
//  extract its first 40-hex sha, decode to a commit sha, ask keeper
//  for that commit's tree sha.
//
//    *out      receives the tree sha on success.
//    *have_out is YES iff `*out` was populated (a missing baseline,
//              missing sha spec, or malformed hex all yield NO without
//              an error).
//
//  Returns ULOGNONE only when the underlying baseline row read fails
//  with ULOGNONE (empty log); other failures collapse to OK + *have=NO
//  so callers that "no baseline → treat as empty tree" stay one-liners.
ok64 SNIFFAtBaselineTreeSha(b8 cur_tip_only, sha1 *out, b8 *have_out);

//  Append every `patch` row's `theirs` sha (decoded from the row's
//  40-hex query or fragment slot) into `out`, oldest-first, walking
//  from the row immediately after the latest `get`/`post` to the
//  ULOG tail.  Stops early once `out` is full (no error).  Empty
//  chain → `out` left empty.  Returns OK; ULOGNONE only when the
//  ULOG has no rows at all.
ok64 SNIFFAtPatchChain(sha1b out);

//  Shape-aware patch-row reader.  Per VERBS.md §POST "Parent /
//  foster / picked assembly", POST needs more than just the sha
//  list — it needs each row's URI shape (squash / cherry-pick /
//  merge / rebase-one) to decide whether to emit a `parent` /
//  `foster` header or a `picked:` trailer, and the user-supplied
//  msg (merge shape only).
//
//  `entries` array is filled oldest-first with up to `cap` rows;
//  on overflow extra rows are dropped (no error).  `*n_out` gets
//  the number written.  Each entry's `msg` slice points into the
//  ULOG buffer — valid until the next ULOG mutation.
typedef struct {
    u8   shape;     //  PATCH_SHAPE_SQUASH/CHERRY/MERGE/REBASE1
    sha1 sha;       //  resolved 40-hex theirs
    u8cs msg;       //  merge: row's fragment; otherwise empty
} sniff_pe;

ok64 SNIFFAtPatchEntries(sniff_pe *entries, u32 cap, u32 *n_out);

//  Timestamp of the latest `post` row, or 0 if none.  This is the floor
//  for "put/delete since last post" scans used by POST's change-set
//  computation.
ron60 SNIFFAtLastPostTs(void);

//  Look up the ULOG row whose ts equals `mtime` (the canonical
//  per-file classifier — `lstat → ron60 → row → verb`).  On OK,
//  `*verb_out` carries the owning verb and `u_out` is URILexer-
//  parsed (slices live in the mmap, valid until ULOGClose).
//  ULOGNONE if no row stamps exactly `mtime`.  Cheap binary search.
ok64 SNIFFAtRowAtTs(ron60 mtime, ron60 *verb_out, urip u_out);

//  Wall-clock guard: refuse if the system clock has moved backwards
//  past the latest log row.  Returns CLOCKBAD when `RONNow() <
//  tail_ts`; OK otherwise (including on an empty log).  Cheap;
//  intended to run once at command entry.
ok64 SNIFFCheckClock(void);

//  Sample a wall-clock timestamp in both ULOG-row and filesystem form.
//  `*ts_out` is the ron60 stamp to append to the log; `*tv_out` is the
//  paired timespec to hand to futimens / utimensat so the file's mtime
//  round-trips through stat() + RONOfTime back to `*ts_out`.  Both
//  values share the same clock_gettime sample (no drift).  The emitted
//  ts is monotonic with respect to the ULOG tail: if the wall-clock
//  reading is not strictly greater than the tail's ts, the helper
//  bumps the returned ts (and the paired timespec) forward by 1 ms.
void SNIFFAtNow(ron60 *ts_out, struct timespec *tv_out);

//  Stamp a file's atime/mtime to `ts` via utimensat, so a later stat()
//  reports an mtime that ron60-encodes to exactly `ts`.  Path is the
//  already-terminated on-disk path.
ok64 SNIFFAtStampPath(path8b path, ron60 ts);

//  CLI's DOGNormalizeArg routes bare tokens (`a.txt`) into the URI's
//  `query` slot rather than `path`, so `put`/`delete` see empty paths.
//  This helper picks the best bytes to treat as the row's path:
//  path → query → fragment → data.  Caller-owned `out` slice; after
//  the call its head/tail point into the caller's URI struct.
void SNIFFAtPathBytes(uri const *u, u8cs out);

//  Iterate every put/delete row whose timestamp is strictly greater
//  than `floor`, in chronological order (oldest first).  The callback
//  receives the full row — `rec->verb` is SNIFFAtVerbPut /
//  SNIFFAtVerbDelete, `rec->ts` is the timestamp, `rec->uri` carries
//  the staged path in `.path` and (for move-form put rows only) the
//  resolved dest path in `.fragment`.  Slices live in the mmap, valid
//  until ULOGClose.  Stops early and propagates the first non-OK
//  callback return.
typedef ok64 (*sniff_at_pd_cb)(ulogreccp rec, void *ctx);
ok64 SNIFFAtScanPutDelete(ron60 floor, sniff_at_pd_cb cb, void *ctx);

//  Pre-resolve a relative `?./X`, `?../X`, or `?..` URI in place.
//  No-op when `u->query` has no relative prefix.  Reads the wt's
//  current branch from `.be/wtlog` baseline and writes the absolute
//  branch path into `qbuf`; rebuilds `u->data` as `?<absolute>` in
//  `databuf`.  Both buffers must outlive the caller's use of `u`.
//  `was_relative_out` (optional) receives YES iff the input had a
//  relative prefix — callers use this to enable create-on-miss.
ok64 SNIFFAtResolveRelativeURI(uri *u, path8b qbuf, u8b databuf,
                               b8 *was_relative_out);

//  Walk the wt rooted at `reporoot` and invoke `cb(rel, ctx)` for
//  every non-meta file whose mtime is NOT in the ULOG stamp-set
//  (i.e. user-edited / unattributed).  `rel` is the on-disk path
//  relative to `reporoot`; its bytes live in FILEScan's iteration
//  buffer and are valid only for the duration of the callback.
//  Used by `sniff status`, GET's cross-branch refusal, and PATCH's
//  refuse-if-dirty pre-flight — every consumer of the stamp-set
//  membership rule.  Stops early on the first non-OK from `cb`.
typedef ok64 (*sniff_at_dirty_cb)(u8cs rel, void *ctx);
ok64 SNIFFAtScanDirty(u8cs reporoot, sniff_at_dirty_cb cb, void *ctx);

//  Extract the current 40-hex commit SHA from a baseline URI.  The
//  canonical get/post row is `?<branch>#<curhash>` — fragment carries
//  the current sha, query carries the be-branch path.  Patch rows
//  put `theirs` (the absorbed commit) in the fragment and have an
//  empty query; pair this helper with `SNIFFAtCurTip` (skips patch
//  rows) when you want the wt's anchor — using it on a patch row
//  returns `theirs`, not `ours`.  Writes the hex bytes into `out`
//  on success; reads the row's fragment first, then falls back to
//  the first 40-hex spec in the query for legacy rows.  Returns
//  ULOGNONE when no such spec is present.
ok64 SNIFFAtQueryFirstSha(uricp u, sha1hex *out);

//  Materialise wt entries as ULOG rows for the heap-merge pipeline:
//  one row per file, `<mtime-ron60>\t<verb><kind>\t<rel>\n`.  The
//  caller passes a verb stem (e.g. SNIFFAtVerbOf("wt")); the scanner
//  appends a kind letter via `ok64sub` — f=regular, x=executable,
//  l=symlink (no submodule on the wt side).  Recover stem with
//  `ok64stem` and kind with `ok64Lit(verb, 0)`.  Fragment is left
//  empty — caller hashes on demand only when classification requires
//  it.  Excludes `.be/`, `.be*`, and IGNO matches; rows arrive in
//  strictly lex-sorted path order (`FILEScanSorted` + `FILEentryZ`,
//  matching git tree sort).  `out` is reset before writing.
ok64 SNIFFWtULog(u8cs reporoot, ron60 verb, u8bp out);

#endif
