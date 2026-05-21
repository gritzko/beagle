# `GRAFGet` — URI-driven deterministic blob/tree merge

## Surface

```c
ok64 GRAFGet(u8b into, u8csc uri);
```

One entry, append-only output, deterministic.  Reaches `&KEEP` and
`&GRAF` through singletons; no handles in the signature.  Caller
opens both dogs beforehand (mirrors `GRAFBlame`).

## URI grammar

    path ( '?' ref ('&' ref)* )?

  - `path` — repo-relative, UTF-8, `abc/PATH.h` rules.  Trailing `/`
    flags tree mode.  Empty path = repo root (tree).
  - `ref` — path-shaped string between `&` separators.  Hex chunks of
    6..40 chars classify as sha prefixes (`DOGIsHashlet`); everything
    else is a branch/tag name, with `./X` / `../X` / `..` resolved
    against the cur branch via `DOGRefAbsolutize`.
  - `n ≥ 1`.  `n == 1` is the degenerate identity case (weave of
    one).

## Semantics

Each `qref` resolves to a commit sha (and thus a `gen`).  The
merged output is the weave projection over the **union** of those
commits' ancestor sets, restricted to commits that touched `path`:

  - token alive iff `intro_gen ∈ U` and (`del_gen == 0` or
    `del_gen ∉ U`), where `U = ⋃ᵢ ancestors(tipᵢ)`.

Commit-sha input (not blob/tree): the weave needs `gen` and
ancestor sets, both of which live on commits.

For tree URIs, the output is a git-format tree object whose each
entry's sha is `sha1(GRAFGet(childpath[/]?tip₁&…&tipₙ))`.  Callers
iterate the tree themselves.

## Steps

### 1. URI split

Use `abc/URI.h` to split `path` / `query`.  Loop `DOGRefDrain` on
the query into a small `u8cs[QGET_MAX]` (cap at 8 — octopus limit).
Trailing-slash → `tree_mode = YES`; strip for path descent.
Extension picked later via `PATHu8sExt` on the final leaf name.

### 2. Ref → commit sha → gen

For each chunk:

  - hex ≥6 (`DOGIsHashlet`) → `KEEPGetExact` (or `KEEPGet` on prefix)
    verify `DOG_OBJ_COMMIT`.
  - otherwise → `REFSResolve` (`keeper/REFS.h`) to the tip commit;
    relative `./X`/`../X`/`..` resolved via `DOGRefAbsolutize`.
  - `gen` via DAG `COMMIT_GEN` lookup; fallback to commit header
    parse if missing.

### 3. Ancestor union

Promote helpers out of `BLAME.c` into `graf/DAG.c`:

  - `DAGAncestorsOfMany(Bwh128 out, dag_stack const *idx,`
    `u64 const *tip_h40, u32 n)` — union of `DAGAncestors` over
    each tip's 40-bit hashlet.
  - `DAGPathVers(blame_ver *out, u32 max, dag_stack const *idx,`
    `u64 path_h40, Bwh128 set)` — lift of `blame_walk_history`,
    shared with blame.
  - `DAG_H40_HEXLEN`, `DAGh40ToKeeperPrefix` — lift of the
    `blame_h40_to_h60_prefix` pair into `DAG.h`.

Fast per-path recompute: one LSM scan per run, filter by
`path_h40` prefix then by ancestor-set membership.  PATH_VER keys
are already 40-bit-path-prefixed; cost is linear in index entries
matching the prefix, not in repo size.

### 4. Blob path (no trailing `/`)

Cardinality-split: `N == 1` is an identity fetch, `N == 2` is a
real 3-way merge via JOIN + DAG LCA, `N >= 3` falls back to the
weave-union approximation (octopus handling TBD).

**N = 1** (identity): `GRAFBlobAtCommit(into, &KEEP, tip, path)`.
No DAG walk; skips weave entirely.

**N = 2** (2-branch merge) — the primary case:

 1. `get_lca(tip_a, tip_b)` — intersect per-tip ancestor sets
    (`DAGAncestors` into two `Bwh128`), pick the intersection
    member with the highest `DAGCommitGen`.  Returns 0 when the
    DAG is empty or the tips share no indexed ancestor.
 2. Fetch `base` blob at the LCA (empty if LCA == 0 or path is
    absent at the LCA — file new on both branches), `ours` blob
    at `tips[0]`, `theirs` blob at `tips[1]`.
 3. `JOINTokenize` each side; `JOINMerge(into, base, ours,
    theirs)` — emits the merged bytes via JOIN's lockstep walk.
    Symmetric disjoint edits both survive; conflicts land as
    inline `<<<<<<<` markers.
 4. When a tip has no blob at `path` (renamed/deleted on one
    side), emit the other side's bytes as-is.  The merge
    degenerates gracefully.

**N >= 3** (octopus): `get_weave_union` — retained as-is for
now.  Per-path `DAGAncestorsOfMany`, `DAGPathVers`, weave replay
oldest-first into an auto-swap `prev`/`cur` blob-buffer pair,
project tokens alive at the union.  This is byte-deterministic
but NOT a true 3-way merge for diverged branches (tokens fed
against the wrong parent collapse — see graf/GET.md §Invariants).
True octopus semantics land when the JOIN path extends to N-way,
or when the weave becomes DAG-parent-aware.

All paths share `GRAFBlobAtCommit` (lifted to `graf/BLOB.c`).

### 5. Tree path (trailing `/`)

In `graf/GET.c`:

 1. For each tip, resolve `path`→tree sha by descending tree
    entries (reuse `blame_tree_step`, lift to `GRAFTreeStep`).
 2. Collect the union of child names across tips (sorted);
    per-name, record `(tip_i, child_sha, child_kind)` for tips
    where the name exists.
 3. For each name:
      - All contributing tips agree on the child sha ⇒ pass that
        sha through.  The entry is resolvable against keeper
        exactly as-is.
      - Shas disagree ⇒ emit a **zero sha** (20 NUL bytes) in the
        entry.  The merged bytes live only in graf's "reproducible
        on demand" semantics (`GRAFGet <path/name[/]?tipN&...>`);
        they have no object in any store, so a synthetic
        `sha1(merged bytes)` would be a fake id nobody can
        dereference.  Zero is the explicit "unresolvable — go ask
        graf" signal.
 4. Mode conflicts (same name, different modes across tips):
    pick the smallest-sha tip's mode deterministically; leave a
    one-line comment in the code explaining why.

Recursion depth is bounded by tree depth; each level owns its
`a_path` stack buffer and a scratch `Bu8` for the child.

### 6. Refactors to land first

  a. `graf/BLOB.c` — `GRAFBlobAtCommit`, `GRAFTreeStep` lifted
     from `BLAME.c`.
  b. `graf/DAG.c` — `DAGAncestorsOfMany`, `DAGPathVers`,
     `DAG_H40_HEXLEN`, `DAGh40ToKeeperPrefix`.
  c. Point `BLAME.c` at the shared helpers; confirm existing
     blame tests pass.

### 7. Public wiring

  - Declare `GRAFGet` in `graf/GRAF.h` next to `GRAFBlame`.
  - Add `graf get <uri>` verb in `graf/GRAF.exe.c` — writes
    `into` to `STDOUT_FILENO`.  No pager.  Doubles as the manual
    test harness.
  - Update `graf/INDEX.md` Files + Verbs tables.

### 8. Tests — `graf/test/`

Table-driven per `CLAUDE.md` §3.

  - `test_get_uri.c` — URI split table: path vs tree-mode,
    SHA/REF mix, `~N`/`^N`, malformed.
  - `test_get_single.c` — `file?c`: bytes equal
    `KEEPGetExact(blob)`.
  - `test_get_linear.c` — `file?c_old&c_new` on one branch:
    bytes equal `c_new` blob.
  - `test_get_merge.c` — two diverged branches, disjoint edits:
    both edits present, ordered by gen.
  - `test_get_octopus.c` — three-way same file.
  - `test_get_tree.c` — `dir/?c1&c2` with disjoint adds: tree has
    union of names, each child sha = `sha1(merged child)`.
  - `test_get_missing_path.c` — path absent in one tip: that tip
    contributes no versions but its ancestors still do.

### 9. Fuzz — `graf/fuzz/`

`fuzz_get_uri.c` — random path + 1..4 tips drawn from a seeded
fixture (pre-built keeper + graf index).  Oracle: result stable
across runs; size bounded by `Σ input_blob_size × k` for small
`k`; no heap leaks under ASAN.  Uses `must()`, not `assert()`.

### 10. Landing order

 6 (refactors) → 4 (blob) → 8 blob-tests → 5 (tree) → 8 tree-tests
 → 9 fuzz → 7 CLI + docs.

## Invariants

 1. **URI determines output byte-for-byte.**  Same URI, same
    keeper + graf index → identical bytes.
 2. **No conflict markers.**  Concurrent inserts sit next to each
    other in `intro_gen` order; rendering is the caller's job.
 3. **No worktree read/write.**  Input is a URI, output is bytes
    into `into`.
 4. **Commit shas only.**  Blob/tree shas are terminal identities
    (`KEEPGetExact` territory), not history inputs.
 5. **Linear per branch.**  Matches `VERBS.md` invariant 2; merge
    provenance already gossiped into REFS contributes to the
    ancestor union naturally.
