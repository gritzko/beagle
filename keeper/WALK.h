#ifndef XX_WALK_H
#define XX_WALK_H

//  WALK: tree walker on a KEEP object store.
//
//  Scope: a single tree rooted at a given SHA-1.  No commit-graph
//  traversal lives here — that is graf/'s purview.
//
//  Two flavors: `WALKTree` (eager — file blobs resolved before the
//  visitor sees them) and `WALKTreeLazy` (no blob resolve; visitor
//  pulls content on demand via `KEEPGetExact`).

#include "abc/INT.h"
#include "abc/RON.h"
#include "KEEP.h"

con ok64 WALKFAIL	= 0x80a5543ca495;
con ok64 WALKNONE	= 0x80a5545d85ce;
con ok64 WALKNOROOM	= 0x80a5545d86d8616;
con ok64 WALKBADFMT	= 0x80a5542ca34f59d;
con ok64 WALKSKIP	= 0x80a554714499;
con ok64 WALKSTOP	= 0x80a55471d619;

//  Tree-entry kind (compressed git mode).  See walk_tree_fn visitor.
#define WALK_KIND_REG 1  // 100644 regular file
#define WALK_KIND_EXE 2  // 100755 executable
#define WALK_KIND_LNK 3  // 120000 symlink
#define WALK_KIND_SUB 4  // 160000 submodule (gitlink)
#define WALK_KIND_DIR 5  // 40000 subtree

//  Classify a git tree-entry mode prefix (the "mode" part of
//  "<mode> <name>\0<sha>") into WALK_KIND_*.  Returns 0 if the
//  mode is unrecognized.
u8 WALKu8sModeKind(u8cs mode);

//  Tree-walk visitor.  Called in depth-first order starting with the
//  root tree itself (empty path, kind=WALK_KIND_DIR), then for each
//  entry of that tree (and, recursively, its subtrees).
//    path  — full relpath from the walk root.  Empty for the root
//            tree visit.  Otherwise no leading '/', no trailing '/'.
//    kind  — WALK_KIND_* (compressed mode).
//    esha  — raw 20-byte SHA-1 of the tree entry (pre-resolve).
//    blob  — for WALK_KIND_REG/EXE/LNK in eager mode: content slice.
//            Empty ($empty()) in lazy mode or for DIR/SUB entries.
//    ctx   — opaque caller context.
//  Return OK to continue, WALKSKIP to skip this entry (don't recurse
//  into a DIR, don't resolve a blob), WALKSTOP to terminate the walk
//  cleanly.  Any other non-OK is a fatal error.
typedef ok64 (*walk_tree_fn)(u8cs path, u8 kind, u8cp esha,
                             u8cs blob, void0p ctx);

//  Hard recursion-depth cap for the shared tree walker (KEEP-001).
//  A git tree nested deeper than this is treated as the end of the
//  walk for that branch (WALKBADFMT) rather than overflowing the C
//  stack.  4096 is far past any real source tree's nesting.
#define WALK_MAX_DEPTH 4096

//  The one keeper tree walker (KEEP-001): depth-first, pre-order over
//  the tree rooted at `root` (20-byte SHA-1), invoking `visit` for the
//  root tree itself (empty path, WALK_KIND_DIR) and then every entry.
//  Bounded recursion — each subtree is walked under its own `try()`
//  frame so per-sibling BASS scratch is rewound, and the descent is
//  capped at WALK_MAX_DEPTH.  `eager` resolves REG/EXE/LNK blobs before
//  the visitor sees them; `!eager` leaves `blob` empty.  All of WALK /
//  WIRECLI-push / CLOSE route their tree descent through this.
//
//  GIT-009: working-tree walks drop the `.be` store-anchor entry (a
//  foreign git history may have committed it as a blob); object-closure
//  walks (push/serve pack-build) MUST still visit it so its blob lands
//  in the pack — the shipped tree bytes reference it, so omitting it
//  yields a dangling ref that a fresh remote's fsck rejects.  Pass
//  `WALK_INCL_ANCHOR` to keep `.be`; `WALK_SKIP_ANCHOR` to drop it.
#define WALK_SKIP_ANCHOR  ((b8)0)
#define WALK_INCL_ANCHOR  ((b8)1)
ok64 KEEPWalkTree(u8cp root, b8 eager, b8 incl_anchor,
                  walk_tree_fn visit, void0p ctx);

//  Walk the tree at `tree_sha` (20-byte) depth-first, eager mode:
//  resolves every REG/EXE/LNK blob through `k` before invoking the
//  visitor, so `blob` is always filled for file entries.
ok64 WALKTree(u8cp tree_sha, walk_tree_fn visit, void0p ctx);

//  Lazy variant of WALKTree.  Never resolves blob objects; `blob` is
//  always empty ($empty()) in the visitor.  Trees are still resolved
//  (required for iteration).  Callers that need a blob can pull it
//  on demand with `KEEPGetExact`.
ok64 WALKTreeLazy(u8cp tree_sha, walk_tree_fn visit, void0p ctx);

//  ls-files: resolve `target` (URI with ?ref or #sha plus optional
//  /subpath) and invoke `visit` once per descendant entry, lazy mode.
//  Paths delivered to the visitor are absolute from the repo root
//  (matching git ls-tree -r output).  If the subpath resolves to a
//  single blob, the visitor is called exactly once for that blob.
ok64 KEEPLsFiles(uricp target, walk_tree_fn visit, void0p ctx);

//  Descend a '/'-separated `subpath` from `root_tree`, segment by
//  segment, matching each segment against the current tree's entries.
//  On success `*out_sha` / `*out_kind` describe the final resolved
//  entry (the root tree itself, as a DIR, for an empty/"."/"./"
//  subpath).  Shared core of PROJ's `tree:`/`blob:` descent and WALK's
//  ls-files descent.
//    pathbuf    — when non-NULL, each matched segment is appended
//                 ('/'-joined) so the caller recovers the resolved
//                 prefix.  Pass NULL to skip the accounting.
//    tbuf       — caller-provided ≥1 MiB scratch (reused per segment;
//                 KEEPGet resets it internally).
//    notfound   — error code returned on a missing / non-tree segment
//                 (PROJ passes PROJNONE; ls-files passes KEEPNONE).
//  Behaviour-preserving extraction of the former proj_descend_inner /
//  lsf_descend_inner (CODE-005).  Does NOT special-case "." / "./" —
//  callers that need the "this-directory" shorthand collapse it to an
//  empty subpath before calling (PROJ does so in proj_descend).
ok64 KEEPTreeDescend(sha1cp root_tree, u8cs subpath, u8bp pathbuf,
                     sha1 *out_sha, u8 *out_kind, u8bp tbuf, ok64 notfound);

//  Materialise a tree's leaf entries as ULOG rows for the heap-merge
//  pipeline: one row per leaf, `<ts>\t<verb><kind>\t<path>#<hex-sha>\n`,
//  sorted by path (DFS == lex order on paths).
//    ts    — caller-provided (commit ts, or 0 if irrelevant).
//    verb  — caller-provided stem (e.g. SNIFFAtVerbOf("base")); the
//            scanner appends a kind letter via `ok64sub`, producing
//            `basef`/`basex`/`basel`/`bases` etc.
//    kind  — RON64 letter encoded into the verb's bottom digit:
//              f = regular file (100644)
//              x = executable file (100755)
//              l = symlink         (120000)
//              s = submodule       (160000) — gitlink, not recursed
//            Recover via `ok64Lit(verb, 0)`; the stem via `ok64stem`.
//    sha   — 40 hex chars (HEXu8sFeed-encoded leaf sha).
//  `out` is reset before writing.  Caller owns it.
ok64 KEEPTreeULog(u8cp tree_sha, ron60 ts, ron60 verb, u8bp out);

//  Tree-vs-tree diff as a ULOG.
//
//  Builds two `KEEPTreeULog` streams (sorted by path / DFS = lex
//  order) over the keeper singleton `KEEP`, runs `ULOGMergeWalk` to
//  group equal-path rows, and writes a fresh ULOG of `add` / `del` /
//  `mod` rows into `out` (caller-owned buffer; reset on entry).
//
//  Output row shapes (all ts = 0; caller may stamp on append):
//      <0>\tadd<k>\t<path>#<new-hex>\n           only in `sha_b`
//      <0>\tdel<k>\t<path>#<old-hex>\n           only in `sha_a`
//      <0>\tmod<k>\t<path>?<old-hex>#<new-hex>\n  both sides, sha differs
//  `<k>` is the kind letter (f/x/l/s) carried in the verb's bottom
//  digit, identical to `KEEPTreeULog`'s convention; recover the stem
//  via `ok64stem`.  Equal paths with equal sha and kind do NOT emit.
//
//  Either `sha_a` or `sha_b` may be NULL — that side is treated as
//  the empty tree (every leaf on the other side fires `add` or `del`).
ok64 KEEPTreeDiff(u8cp sha_a, u8cp sha_b, u8bp out);

// --- Range banner renderers (commit list + per-file diff) ----------
//
//  Shared output for the "what moved" banner: GET prints it on
//  checkout; POST(-push) and PATCH print it for the range they
//  advance.  Rows go through `ULOGPrintStatusLine` (stdout, active
//  HUNKMode) so they match sniff's bare-`be` status shape.  Both are
//  best-effort: a missing / garbled object emits nothing and still
//  returns OK, so a banner loop never aborts on one bad row.

//  Emit one `post\t?<hashlet8>#<subject>` status row for `commit_sha`.
//  `fallback_ts` stamps the row when the commit body carries no author
//  time.  Non-commit / absent object → no output.
ok64 KEEPEmitCommitLine(sha1cp commit_sha, ron60 fallback_ts);

//  Resolve `tgt_commit` (and `base_commit`, NULL = empty tree) to their
//  root trees and emit one `<add|del|mod>\t<path>` status row per
//  differing leaf (via `KEEPTreeDiff`).
ok64 KEEPEmitTreeDiffFiles(sha1cp base_commit, sha1cp tgt_commit,
                           ron60 fallback_ts);

//  Emit one commit row (via KEEPEmitCommitLine, newest-first) for every
//  commit reachable from `tip` (parent edges) that is NOT already
//  reachable from `base` (parent + foster edges) — i.e. the commits a
//  squash/merge of `tip` introduces over a wt that already holds
//  `base`.  `base` NULL ⇒ `tip`'s whole closure.  This is a direct
//  keeper walk (no graf DAG) so it is deterministic mid-command.
//  Bounded at KEEP_RANGE_CAP commits each side (best-effort banner).
ok64 KEEPEmitCommitsSince(sha1cp base, sha1cp tip, ron60 fallback_ts);

#endif
