#ifndef GRAF_GRAF_H
#define GRAF_GRAF_H

#include "abc/INT.h"
#include "abc/KV.h"
#include "abc/MSET.h"
#include "dog/CLI.h"
#include "dog/DOG.h"
#include "dog/HOME.h"
#include "dog/HUNK.h"
#include "dog/git/SHA1.h"
#include "graf/DAG.h"
#include "graf/WEAVE.h"

#define GRAF_ARENA_SIZE (1UL << 24)   // 16MB
#define GRAF_OBJ_BUF_SIZE  (1UL << 20)   // 1MB — commit/tree object body

// Forward decl for transient ingest state (see graf/DAG.c).
typedef struct dag_ingest dag_ingest;

// --- graf control struct (per DOG.md rule 8) ---
//
// Single-shard layout: one per-project dir `<root>/.be/<project>`
// holds every `<seqno>.graf.idx` file, registered as a DOGPup* puppy
// stack (`puppies`).  Branch is pure ref context (`h->cur_branch`),
// not a directory.  GRAFOpenBranch scans that one dir via
// DOGPupOpenAll; all runs are always visible (PAST stays empty).  The
// typed `wh128cs runs[]` view is rebuilt from the puppy stack on every
// Open and after each DOGPupCreate / DOGPupThinTail; queries (DAGLookup
// et al) consume it via `GRAFRuns()`.

typedef struct {
    home        *h;          // borrowed
    int          lock_fd;    // flock on project shard dir's .lock; -1 = ro
    Bu8          arena;      // hunk staging buffer (renderer only)
    //  Per-call scratch for the (commit, path) → blob walk.  Reused
    //  across `GRAFBlobAtCommit` (`obj_buf`: commit object body) and
    //  `GRAFTreeStep` (`tree_buf`: one tree-object body).  Not
    //  reentrant — callers in the same call stack must not overlap
    //  usage of the same buffer.  Reset on entry, kept mapped for the
    //  lifetime of GRAFOpen.
    Bu8          obj_buf;
    Bu8          tree_buf;
    int          out_fd;     // output fd (-1 = uninitialized)
    b8           keep_owned; // YES iff GRAFOpenBranch opened keeper (vs
                             // finding it already open) → GRAFClose owns
                             // the matching KEEPClose.  Resource lifecycle
                             // lives in Open/Close, never in GRAFExec.

    //  Puppy stack: (pup_key → fd) for every `<pup_key>.graf.idx` in
    //  the single project shard dir.  Pup keys are 60-bit ron60 values.
    //  Mmaps live in FILE_WANT_BUFS[fd].  Compaction appends a new puppy
    //  to the tail (DOGPupCreate) and drops the young suffix
    //  (DOGPupThinTail).
    Bkv64        puppies;
    //  Active leaf-branch path lives in `h->cur_branch`.  Graf's
    //  switch hook runs BEFORE keeper updates `h->cur_branch` (per
    //  the dog dep graph: graf depends on keeper, so callers invoke
    //  graf first while h->cur_branch still reflects the old leaf).
    u64          last_pup_key; // monotonic high-water mark for fresh
                               // `.graf.idx` pup_keys; minted via
                               // `max(RONNow(), last_pup_key + 1)`.
                               // Seeded from on-disk + loaded max at
                               // open.

    //  Typed wh128cs view over `puppies`, rebuilt by `graf_refresh_view`
    //  on every change.  Newest run sits at the highest index — query
    //  loops scan in reverse so newer LSM levels override older.
    wh128cs      runs[MSET_MAX_LEVELS];
    u32          runs_n;

    dag_ingest  *ing;        // lazily allocated on first GRAFDagUpdate
} graf;

//  Singleton.  Zero-initialised; populated by GRAFOpen.
extern graf GRAF;

// --- Internal helpers used by the indexer (implemented in DAG.c) ---
ok64 GRAFDagUpdate(u8 obj_type, sha1cp sha, u8cs blob);
ok64 GRAFDagFinish(void);

// --- Error / sentinel codes ---

con ok64 GRAFFAIL    = 0x41b28f3ca495;
con ok64 GRAFOPEN    = 0x41b28f619397;
con ok64 GRAFOPENRO  = 0x41b28f6193976d8;
//  Branch dir not found.  No longer raised by graf itself under the
//  single-shard layout (every run lives in the one project dir);
//  retained because GRAF.exe.c / LOG.c still match on it as a
//  "stay on cur" signal from downstream resolvers.
con ok64 GRAFNOPATH  = 0x41b28f5d864a751;
//  No `--at` anchor available — `diff:` projector forms that need a
//  baseline (any URI without an explicit `?h1..h2` range) refuse with
//  this rather than silently falling back to "wt as base".
con ok64 GRAFNOAT    = 0x41b28f5d829d;
//  Resolver output buffer too small for the canonic URI bytes.
con ok64 GRAFFULL    = 0x41b28f59d54d;
// --- Public API (DOG 4-fn, singleton) ---

//  Open graf state on the trunk.  Thin wrapper over `GRAFOpenBranch`
//  with an empty branch.  Returns OK (I opened), GRAFOPEN (already
//  open compatible), GRAFOPENRO (ro/rw conflict), or a real error.
ok64 GRAFOpen(home *h, b8 rw);

//  Branch-aware Open.  Normalizes `branch` via DPATHBranchNormFeed,
//  registers it on the home singleton via HOMEOpenBranch, then scans
//  the single project shard dir `<root>/.be/<project>`, calling
//  `DOGPupOpenAll(GRAF.puppies, dir, ".graf.idx")` once.  Branch is
//  pure ref context (`h->cur_branch`), not a directory.  Locks the
//  shard dir's `.lock` when rw.  Refreshes the typed `runs[]` view.
//  Mirrors `KEEPOpenBranch`.
ok64 GRAFOpenBranch(home *h, u8cs branch, b8 rw);

//  Re-target an open graf from current branch to `new_branch` WITHOUT
//  closing.  Single-shard layout makes this label-only: every
//  `.graf.idx` run already lives in the one project dir loaded at
//  open, so the switch just re-points `h->cur_branch` (via
//  HOMESetCurBranch).  No dir walk, no PAST/DATA flip, no rescan, no
//  lock swap.  Mirrors `KEEPSwitchBranch`.  Cross-branch DAG walks
//  (POSTPromote-style rebases, located cherry-pick) already see every
//  run.
ok64 GRAFSwitchBranch(home *h, u8cs new_branch);

//  Fill `out` with a live `wh128css` view over the open puppy stack
//  (oldest run at index 0, newest last).  Slice ends point into
//  `GRAF.runs[]`; valid until the next DOGPupCreate / DOGPupThinTail
//  / GRAFClose.
void GRAFRuns(wh128cssp out);

//  Refresh GRAF.runs[] / GRAF.runs_n from the current GRAF.puppies
//  stack.  Called by GRAFOpenBranch and after every DOGPupCreate /
//  DOGPupThinTail; exposed here so DAG.c's compactor (which lives
//  next to its writes) can keep the view in sync.
void GRAFRefreshView(void);

//  Create a new `.graf.idx` file in `dir` with a globally-unique
//  ron60 pup_key (`graf_next_pup_key`).  Mirrors keeper's
//  `keep_pup_create_next`: every fresh idx run gets a pup_key that
//  clears EVERY shard dir on disk, so siblings can't collide.
//  Drop-in replacement for `DOGPupCreate(g->puppies, dir, ext, data)`.
ok64 GRAFPupCreateNext(path8s dir, u8cs ext, u8cs data);

//  Run one CLI invocation.
ok64 GRAFExec(cli *c);

//  Close singleton; idempotent.
ok64 GRAFClose(void);

//  Verb + value-flag tables for CLIParse.
extern char const *const GRAF_CLI_VERBS[];
extern char const GRAF_CLI_VAL_FLAGS[];

// --- Output sink globals.  Bytes flow through `HUNKu8sFeedOut`
//     dispatched off the module-global `HUNKMode`; graf owns the
//     scratch arena and the destination fd. ---

extern Bu8          graf_arena;
extern int          graf_out_fd;

ok64 GRAFArenaInit(void);
void GRAFArenaCleanup(void);

// Serialize one hunk via graf_emit and write to graf_out_fd.
ok64 GRAFHunkEmit(hunk const *hk, void *ctx);

// Emit `diff:?<hex>` into `out` after the just-rendered anchor bytes
// and pack a 'U' tok past those bytes via `toks`.  Bytes are
// zero-width in the renderer; bro's click handler executes
// `be --tlv diff:?<hex>` on left-click of the preceding anchor.
// No-op when `toks` is the zero slice (plain-text mode).  `hex`
// may be any length (10 for a 40-bit hashlet, 40 for a full sha).
void GRAFEmitDiffUri(u32b toks, u8b out, u8cs hex);

// 3-way merge entry.
ok64 GRAFMerge(u8cs base_path, u8cs ours_path, u8cs theirs_path,
               u8cs outpath);

// Drive a full streaming ingest from keeper: iterate every commit in
// the keeper store, replay one (COMMIT, sha, body) into graf's DAG,
// and finalize.  Used by `graf index` (no URI) for forced reindex.
ok64 GRAFIndex(void);

// Walk back from each tip in `u` (resolved via GRAFResolveTip) over
// COMMIT_PARENT edges in keeper, calling GRAFDagUpdate per commit and
// stopping per-branch when the next commit is already in graf's own
// index (DAG.md: mention ≡ known).  Multi-tip URIs and ranges are
// folded together; bare URI (no query) walks the wt's current tip.
// Used by `graf get URI` under the new arrangement (DOG.md §10a).
ok64 GRAFIndexFromTips(uricp u);

// Token-level blame (reads blobs from keeper).
//   tip_h: 40-bit commit hashlet bounding the history (0 = no filter).
ok64 GRAFBlame(u8cs filepath, u64 tip_h, u8cs reporoot);

// Build the file's full token weave by replaying its blob history along
// `tip_h`'s ancestor closure (oldest-first, byte-dedup adjacent), then
// optionally folding in the on-disk worktree bytes as a final layer
// tagged `wt_src`.  Caller owns three weave instances (init'd via
// `WEAVEInit`); the one holding the final state is returned in
// `*out_final` (points into `wsrc` or `wdst`).  `cb`, when non-NULL, is
// invoked once per kept layer with its `src_id` (truncated commit
// hashlet, or `wt_src` for the wt layer) and the full 64-bit hashlet
// (0 for the wt layer).
//
//   tip_h    : 40-bit commit hashlet bounding history (0 = all).
//   wt_src   : `WEAVE_WT_SRC` to fold wt as a final layer; 0 to skip.
typedef ok64 (*GRAFweaveStepCb)(u32 src_id, u64 commit_h, void *ctx);

ok64 GRAFFileWeave(weave *wsrc, weave *wdst, weave *wnu,
                   weave **out_final,
                   keeper *k, u8cs filepath, u64 tip_h,
                   u8cs reporoot, u32 wt_src,
                   GRAFweaveStepCb cb, void *cb_ctx);

// Linear-chain analogue of `GRAFFileWeave`.  For each commit in
// `chain[0..nchain)` (oldest-first), fetches the blob at `filepath`
// and folds it into the weave via `WEAVEFromBlob` + `WEAVEDiff` with
// `src` set to the low 32 bits of `WHIFFHashlet40(commit_sha)`.
// Adjacent commits whose blob bytes match the prior kept version are
// silently dropped; commits where `filepath` is absent are skipped.
// No DAG dependency, no worktree layer.  Same three-buffer caller
// pattern as `GRAFFileWeave` (`wsrc` / `wdst` / `wnu`); the buffer
// holding the final state is returned in `*out_final`.  `cb`, when
// non-NULL, fires once per kept layer with `src_id` = the same
// 32-bit hashlet stamped on the layer and `commit_h` =
// `WHIFFHashlet60(commit_sha)`.
ok64 GRAFRebaseFileWeave(weave *wsrc, weave *wdst, weave *wnu,
                         weave **out_final,
                         keeper *k, u8cs filepath,
                         sha1cp chain, u32 nchain,
                         GRAFweaveStepCb cb, void *cb_ctx);

// Single-blob WEAVE merge for the rebase replay loop.  `running` is
// the file's weave on the post-rebase head's history; `branch` is the
// file's weave on the to-be-replayed commit's history.  Both must
// share an NCA-bootstrap layer (`src=0`) so WEAVEMerge can recover
// the spine.
//
// `WEAVEMerge` combines the two histories; `WEAVEEmitMerged` then
// renders the alive byte stream into `out` (reset before writing),
// using `in_running` / `in_branch` as the per-side membership
// predicates over 32-bit commit hashlets — typically backed by each
// side's `DAGAncestors` closure.  When the rendered run has tokens
// whose memberships are disjoint, `WEAVEEmitMerged` frames the
// divergence with `<<<<` / `||||` / `>>>>` JOIN-format markers; the
// post-render scan sets `*out_conflict = YES` and the caller maps
// that to `GRAFCNFL`.  No keeper IO; pure weave/predicate work.
ok64 GRAFRebaseBlobMerge(u8cs base, u8cs ours, u8cs theirs, u8cs ext,
                         u8 *const *out, b8 *out_conflict);

// Resolve a URI's `#hex` / `?ref` / absent-query to a 20-byte commit
// SHA-1 — the same policy `log:` uses (sniff/at.log → REFS fallback).
ok64 GRAFResolveTip(uricp u, sha1 *out);

//  Branch-first disambiguation probe (URI-001 §"The one rule").
//  OK iff `ref` resolves as a REFS NAME (branch / tag / wire-prefixed
//  ref) in the open keeper store; GRAFNONE otherwise (bare sha,
//  no keeper, miss).  Lets the diff / blame ref-URI composers pick
//  `?<name>` over `#<sha>` branch-first, so a ref whose name is all
//  hex still wins over a same-spelled hashlet.  Best-effort: callers
//  treat any non-OK as "not a name" and fall back to the syntactic
//  hashlet test.  Requires GRAFOpen / KEEPOpen on the same home.
ok64 GRAFRefIsName(u8cs ref);

//  Universal version resolver.  Takes any user-supplied URI shape
//  and rewrites the `?query` slot into the canonic form
//      ?/<project>.<hashlet>/<branch-path>/<sha-or-tag>
//  (see https://replicated.wiki/html/wiki/URI.html §"URI structure").  Magic refs (`?null`, `?back`),
//  trailing-slash branch/tag rules, project-relative paths,
//  hashlets, commit-message search by whitespace shape — all
//  handled here, in one place, so callers downstream of
//  GRAFResolveVersion see only canonic input.
//
//  Reads `given` (not consumed); feeds canonic bytes into `canonic`
//  (writable slice; caller pre-sizes to MAX_URI_LEN).  The
//  scheme / authority / path / fragment slots of `given` pass
//  through verbatim — only the `?query` is rewritten.  Fragment
//  is opaque verb payload; never reinterpreted here.
//
//  Open-once: requires GRAFOpen(home*) to have been called by the
//  caller's frame so refs / DAG lookups can hit disk.
//
//  Returns:
//    OK         — canonic written; bytes in `canonic` are the
//                 rewritten URI.  Idempotent on already-canonic
//                 input (copy-through).
//    GRAFNONE   — no resolution exists (search returned zero,
//                 magic `?back` with empty reflog, ref missing).
//                 Caller decides whether to escalate to a full-
//                 project fallback scan (see TRIANGLE.todo.md).
//    GRAFFULL   — `canonic` is too small for the rewritten bytes.
//    GRAFFAIL   — data error (refs unreadable, no project context).
ok64 GRAFResolveVersion(u8s canonic, u8csc given);

// Weave diff between two commits (reads blobs from keeper).
ok64 GRAFWeaveDiff(u8cs filepath, u8cs reporoot,
                   u8cs from, u8cs to);

// URI-driven diff primitives.  Each emits one hunk block per changed
// file through `GRAFHunkEmit`.
//
//   GRAFDiffWtFile   — single file: weave-based diff of wt-on-disk
//                      against the baseline blob at `base_h40`
//                      (40-bit commit hashlet from sniff's `--at`
//                      anchor).  wt is folded into the file's weave
//                      as the next version after the base, so
//                      attribution is preserved.
//   GRAFDiffWtTree   — whole tree: walk the base tree, run a per-file
//                      weave diff against wt for each file.  `base_hex`
//                      is the 40-hex spelling of the same commit (used
//                      to compose the URI for `KEEPLsFiles`).  wt-only
//                      additions are not yet emitted.
//   GRAFDiffTreeRefs — whole tree: walk both refs, pair by path,
//                      orphans on either side become deletions or
//                      insertions against empty.
ok64 GRAFDiffWtFile(u8cs filepath, u64 base_h40, u8cs reporoot);
ok64 GRAFDiffWtTree(u64 base_h40, u8cs base_hex, u8cs reporoot);
ok64 GRAFDiffTreeRefs(u8cs from, u8cs to, u8cs reporoot);

// 2-layer weave diff: WEAVEFromBlob ×2 + WEAVEDiff (LCS+NEIL+canon) +
// WEAVEEmitDiff.  The single engine every diff path uses.  `name` is
// the hunk title (file path); `ext` selects the tokenizer; either
// blob slice may be empty (file added or deleted).  No-change pair →
// no hunks emitted.
ok64 GRAFDiff2Layer(u8cs name, u8cs ext, u8cs from_data, u8cs to_data);

// Deterministic URI-driven blob/tree merge (see graf/GET.md).
//
// URI grammar: `path?sha1&sha2&...&shaN`.  Trailing `/` on the path
// selects tree mode (future task).  Reaches keeper via &KEEP and graf
// state via &GRAF — callers must have both singletons open.
ok64 GRAFGet(u8b into, u8csc uri);

// Weave-merge a single file across two commits, treating the wt's
// on-disk bytes for `path` as an implicit edit attached to `base`.
// Builds the ancestor-closure weave per tip via `build_tip_weave`,
// folds the wt bytes as a final WEAVE_WT_SRC layer on the base side,
// runs WEAVEMerge, and writes the resulting alive-token bytes into
// `out` (reset before writing).  Reads keeper through `&KEEP`.
//
// Both `base` and `tgt` are 20-byte commit SHAs; converted internally
// via `WHIFFHashlet40` to drive graf's history walks.  Caller writes
// `out` to disk and stamps the new mtime.
//
// Returns OK on success; GRAFFAIL when neither side's history has
// any alive tokens for `path` (path absent on both); other errors
// propagated from the weave build / merge / emit steps.
//
// Note: divergent regions currently render as base-then-tgt
// concatenation — WEAVE marker emission is a follow-up.  Callers
// that need conflict signaling should diff the result against tgt.
ok64 GRAFMergeWtFile(u8cs path, u8cs reporoot,
                     sha1cp base, sha1cp tgt,
                     u8b out);

//  Tunable variant of GRAFMergeWtFile.  Both sides' ancestor closures
//  are walked under the same DAG_EDGE_* `edges` bitmask + the same
//  `skip_hl` set (caller-supplied 60-bit hashlets to omit; pass
//  nskip=0 / NULL for none).  GRAFMergeWtFile is the same as calling
//  this with `edges = DAG_EDGE_PARENT` and `nskip = 0`.
//
//  PATCH.c uses `DAG_EDGE_PARENT | DAG_EDGE_FOSTER` for squash /
//  merge / rebase-one so that prior `?br#`+post cycles' absorbed
//  trunk commits (attached via `foster` headers) participate in
//  reachability — without it, repeated rebases see the same blob
//  arriving from "two independent introductions" and produce
//  duplicate / conflict-marked output.
ok64 GRAFMergeWtFileTunable(u8cs path, u8cs reporoot,
                            sha1cp base, sha1cp tgt,
                            u32 edges,
                            u64 const *skip_hl, u32 nskip,
                            u8b out);

//  3-way blob merge from raw bytes (no keeper / no DAG walk).
//  Pipeline: WEAVEFromBlob ×3 → WEAVEDiff ×2 → WEAVEMerge →
//  WEAVEEmitMerged.  Marker shape: `<<<<` / `||||` / `>>>>` with the
//  1/4-line realignment pass applied.  Empty `base` is allowed
//  (no common ancestor); empty `ours` or `theirs` short-circuits to
//  the other side.  `out` is reset on entry.  `ext` selects the
//  tokenizer (file extension, no dot).
ok64 GRAFMerge3Bytes(u8cs base, u8cs ours, u8cs theirs,
                     u8cs ext, u8b out);

// Render commit history one-per-line for `be log:[path]?<ref>#<N>`.
// Branch-only URI (no path) walks the COMMIT_PARENT chain via the DAG
// index; path-bearing URI uses PATH_VER + ancestor filter.  Output
// rides graf_emit (TLV via bro on TTY, raw text otherwise).
ok64 GRAFLog(uricp u);

// `graf head '#<msg-substring>'` — walk cur's first-parent chain
// (cur tip parked by HOMEOpen via `--at`), substring-match the
// commit-message body, emit the first matching commit's log row.
// Returns GRAFNONE when no commit matches; FAILSANITY on bad URI;
// GRAFFAIL on transport / object-fetch error.  Used by `be head
// '#parallel'` (https://replicated.wiki/html/wiki/HEAD.html §HEAD).
con ok64 GRAFNONE    = 0x41b28f5d85ce;
ok64 GRAFHead(uricp u);

// Subway-map view of the branch tree for `be map:`.  Reads from the
// keeper + graf singletons; caller has already opened both.  Phase 1
// emits one indented row per branch within the current branch's
// ancestors+descendants window; Phase 2/3 will replace the indent
// with subway glyphs and weave in commit-timeline rows.
ok64 GRAFMap(uricp u);

// Latest common ancestor of two commits.  Intersects each tip's
// `DAGAncestors` set, picks the member with the highest `gen`, then
// recovers the full 20-byte commit sha via `KEEPGet`.  `out` left
// zeroed when the DAG is empty or the tips share no indexed
// ancestor; caller treats that as "unrelated histories" (refuse a
// merge, fall back to ours, etc).
ok64 GRAFLca(sha1 *out, sha1cp a, sha1cp b);

#endif
