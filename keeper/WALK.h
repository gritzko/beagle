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

#endif
