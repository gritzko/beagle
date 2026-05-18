# graf/ — token-level diff, merge, blame, history indexing

`graf` is a dog: it owns `.be/` under a repo root and speaks
the three-function contract from [dog/DOG.md](../dog/DOG.md) §8
(`DOGOpen` / `DOGExec` / `DOGClose`).

Indexing is **pull-based** (DOG.md §10a): under the new arrangement
`be get URI` spawns `graf get URI` in parallel with keeper, and
graf walks the URI's tip(s) back through keeper's read APIs over
COMMIT_PARENT edges, stopping per-branch at any commit already in
its own DAG (mention ≡ known).  `graf index` (no URI) remains as a
forced full reindex.  No git CLI; no `.git/`; the only source of
object data is keeper's pack store.

## Verbs

```
graf get    path?sha1&sha2[&...]   URI-driven deterministic blob merge
graf get    URI (any other shape)  tip-walk indexer (incremental)
graf merge  base ours theirs       3-way merge; -o <file> to write out
graf blame  path                   token-level blame (reads keeper + DAG)
graf weave  path?from..to          weave diff across a ref range
graf log    [path]?ref[#N]         commit history, one per line
graf head   '#<msg-substring>'     find cur-reachable commit by msg
graf index                         full reindex (force, ignore stop-set)
graf status                        index run/entry counts
```

## Files

| File          | Purpose |
|---------------|---------|
| `GRAF.h`      | Singleton state, arena, `graf_emit`, public API. `GRAFOpenBranch(h, branch, rw)` walks trunk → leaf; missing prefix dirs return `GRAFNOPATH` |
| `GRAF.c`      | `GRAFOpen` / `GRAFOpenBranch` / `GRAFClose`, arena init, `GRAFHunkEmit` |
| `GRAF.exe.c`  | `GRAFExec` — verb dispatch (get / diff / merge / blame / weave / index / status) |
| `GRAF.cli.c`  | `main()` — parse argv, open singleton, call `GRAFExec` |
| `BLOB.{h,c}`  | `GRAFTreeStep` / `GRAFBlobAtCommit` — shared (commit, path)→blob resolution via keeper, used by BLAME and GET |
| `DAG.{h,c}`   | LSM of `wh128` records under `.be/` driven by `GRAFDagUpdate`.  Each `wh128` half (`key`, `val`) carries a 60-bit hashlet plus a 4-bit per-half type (`COMMIT=1`, `TREE=2`).  Entry kinds = `(key.type, val.type)` pairs: `(COMMIT,COMMIT)` parent edge and `(COMMIT,TREE)` root-tree edge.  Per-tree-entry edges are NOT recorded — git pack-side delta compression keeps tree storage cheap while materialising every entry here would dominate the repo footprint.  Path resolution at query time goes through keeper directly (`graf/BLOB.c::GRAFTreeStep`).  Hashlets are 60-bit (top of SHA-1) — same width keeper uses, so there's no prefix-lift step.  `DAGCommitTree` / `DAGParents` / `DAGAncestors` / `DAGAncestorsOfMany` / `DAGAllCommits` / `DAGTopoSort` are the public navigation surface |
| `GET.c`       | `GRAFGet(u8b into, u8csc uri)` — URI-driven deterministic blob/tree merge. Blob mode (`file?sha1&sha2`): N=1 identity via `GRAFBlobAtCommit`; **N=2 true 3-way merge** via `get_lca` (DAG ancestor intersection, max-gen) + `JOINMerge`; N≥3 falls back to a weave-union approximation (`DAGAncestorsOfMany`, `DAGPathVers`, linear oldest-first replay). Tree mode (`dir/?sha1&sha2`, `/?sha...` for root): resolves each tip's tree at `path`, unions child names across tips, recurses via GRAFGet on disagreeing children (hashing merged bytes as blob or tree via `KEEPObjSha`), emits git-tree-format bytes (`<mode> <name>\0<20-byte sha>`) sorted bytewise by name — see graf/GET.md |
| `JOIN.{h,c}`  | `JOINTokenize` / `JOINMerge` — 3-way merge primitive over u64-hash token streams via abc/DIFFx LCS |
| `REBASE.{h,c}` | Linear-history replay primitives for the upcoming POST rewrite (Stage 2): `GRAFPatchId(commit_body)` (stable u64 diff-id vs first parent, RAPHashSeed-folded over sorted (path, parent_sha, child_sha) tuples; 0 for root/empty/error so dedup never matches), `GRAFMergeExplicit(base, ours, theirs, out)` (3-way blob merge via `KEEPGetExact` + `JOINMerge`, base supplied directly — bypasses LCA walk), `GRAFRebase(base_old, base_new, child_tip, cb, ctx)` (replay child_tip → base_old onto base_new, oldest-first, patch-id dedup against base_new ancestors, conflict aborts with `GRAFCNFL`, all object emission goes through caller-supplied `graf_rebase_emit_cb`). Keeper-read-only — no DAG dependency, no keeper mutation |
| `NEIL.{h,c}`  | Edit-list semantic cleanup (called from `WEAVE.c`): removes false short equalities, lossless boundary shifts |
| `BRAM.{h,c}`  | Patience diff over u64 token-hash arrays (Cohen 2003).  Drop-in for `DIFFu64s` when callers want line-coherent alignment (anchor lines = hash unique on both sides) so token-level Myers can't mis-align across repeated `}` / boilerplate |
| `MERGE.c`     | `GRAFMerge` — 3-way merge using `JOIN`, writes resolved bytes to file or stdout |
| `DIFFREF.c`   | URI-driven diff (`diff:`/`be diff` projector): file or whole tree, ref-vs-wt or ref-vs-ref.  Builds 2-layer weave per file (via `WEAVE.c`) and emits unified-diff hunks |
| `INDEX.c`     | DAG-ingest drivers: `GRAFIndex(k)` full reindex over keeper's commit objects + `GRAFIndexFromTips(k, u)` incremental walk back along `COMMIT_PARENT` edges, stopping per-branch when the parent is already in graf's DAG |
| `MAP.c`       | `GRAFMap` — subway-map view (`be map:`).  Multi-column commit-history render: filters to cur's ancestors-to-trunk + descendants, time-sorts, draws per-branch spine glyphs (`║` trunk / `┃` child / `│` grandchild+) |
| `BLAME.c`     | `GRAFFileWeave` (shared file-history weave builder: ancestor-closure walk, byte-dedup, optional wt-as-final-layer with `WEAVE_WT_SRC`, per-layer step callback) + `GRAFBlame` (renders attribution rows over the built weave) + `GRAFWeaveDiff` |
| `WEAVE.{h,c}` | Double-buffered weave of token versions with intro/del gens.  `WEAVEDiff` runs `DIFFu64s` over alive-src vs nu hashlets, then `NEIL.Cleanup` + `NEIL.Shift` over the EDL (alive-only text/toks views are materialised once per step) so spurious whitespace/punctuation EQ matches between unrelated lines don't pollute the weave's `inrm` attributions.  `WEAVEMerge` (3-way concurrent-branch merge) diffs the *full* hashlet streams of two derived weaves with each token's `inrm.in` mixed into the diff key (Knuth multiplier + `in`) so Myers can't spuriously align same-byte tokens of different provenance — the LCS recovers the shared spine cleanly when both inputs come from a common ancestor via `WEAVEDiff`.  EQ runs reconcile `(in, rm)` per-token (deleter wins; alive-on-both takes `min(in_a, in_b)`); non-EQ runs canonicalise as INS-then-DEL with each side's tokens carrying their original `inrm`.  When both sides have *alive* inserts at the same logical slot AND the inserted bytes differ, synthetic conflict-marker tokens (`<<<<` / `||||` / `>>>>`, src = `WEAVE_CFLCT_SRC`) frame the divergence — alive on output so downstream `has_conflict_marker` detectors keep working.  `WEAVEEmitDiff` walks a built weave and emits unified-diff hunks with context (3 lines on each side, clusters merge through gaps ≤ 2×CTX) classified by caller-supplied `(in_from, in_to)` predicates.  Each hunk carries `text` + `toks` (lexer syntax tags from the weave) + `hili` (`I`/`D`/`' '`) |
| `LOG.c`       | `GRAFLog` + `GRAFHead`.  `GRAFHead` is `be head '#<msg>'` (VERBS.md §HEAD): walks cur's first-parent chain via the DAG COMMIT_PARENT index, fetches each commit body via keeper, emits the first commit whose message body contains the fragment as a substring.  Bounded by `GRAFHEAD_MAX_WALK` (65536); no-match returns `GRAFNONE`. `GRAFLog` — `be log:[path]?ref[#N]` projector. Branch history walks `(COMMIT,COMMIT)` parent edges via the DAG index; file history (`./path/file?ref`) topo-sorts the tip's ancestor closure and emits a row whenever the blob bytes at `path` differ from the prior commit's. Commit body fetched from keeper for the `<sha7> <date> <author> <summary>` render |

## Pager

When diff/weave/blame runs with a tty stdout, graf forks `bro`
(resolved via `HOMEResolveSibling`) and writes TLV hunks to the
pipe.  With non-tty stdout, graf writes plain ASCII via
`HUNKu8sFeedText` directly.  `merge` does not page.
