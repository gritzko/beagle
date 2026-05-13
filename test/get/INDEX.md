# get/ — `be get` (checkout) integration cases

* `01-checkout/` — put + post + `be get '?'` on a fresh repo with one
  file (greet.txt = "hello world\n"); pins down post stderr shape and
  that GET succeeds against the just-committed branch tip.
* `02-single-file-overwrite/` — `be get file.c?feat` (VERBS.md §GET):
  fork ?feat with a different `lib.c`, switch back to trunk, then
  pull only `lib.c`'s feat-side blob into the wt.  Asserts the file
  bytes flip and `.be/wtlog` does NOT grow (no staging — no `get` row).
* `03-subtree-overlay/` — `be get src/?feat` (trailing slash, VERBS.md
  §GET).  Overlays every leaf under `src/` from feat's tip into the
  wt; files outside `src/` (here `common.txt`) and `.be/wtlog` row count
  stay put.  Exercises modified leaves + an added leaf.  No prune.
* `05-submodule-from-ssh/` — `be get ssh://localhost/<rel>/parent.git?master`
  on a parent that carries a `160000` gitlink + `.gitmodules` blob
  pointing at a sibling sub repo.  Asserts the sub mount materialises
  at `vendor/sub/` (with a regular `.be` anchor file), the sub store
  placeholder lands at `.be/sub/` (basename-keyed), and `vendor/sub/lib.h`
  carries the upstream blob.  Gated on `WITH_SSH`.  Exercises
  MODULES.plan.md Phases 1 + 3 end-to-end.
* `07-submodule-nosub/` — `be get --nosub ssh://localhost/<rel>/parent.git?master`
  on the same submodule-bearing parent as case 05.  Asserts the
  parent files materialise but the sub mount loop is short-circuited:
  no `vendor/sub/lib.h`, no `.be/sub/` store, and stderr carries
  the `submodule(s) skipped (--nosub)` line.  Regression spec for
  the conflict-marker breakage at sniff/GET.c around the `--nosub`
  branch.  Gated on `WITH_SSH`.
* `06-branch-switch-shards/` — `be get ?<branch>` switches the wt
  across sibling branches, keeping each branch's packs inside its
  own `.be/<branch>/` shard.  Builds trunk + ?feat + ?other (both
  off trunk), bounces wt ?other → ?feat → ?other, and asserts
  files round-trip + each branch dir owns its own pack.  Exercises
  the PAST/DATA partition (KEEP.h §"Branch-aware object store"),
  `KEEPSwitchBranch`, and the global-seqno scan that keeps sibling
  branches from colliding on file_id.  WILL_FAIL today — sniff
  still opens keeper on trunk so cross-branch POST / PATCH ops
  keep working via the flat-pool semantics.  Flip on after POST /
  PATCH route through `KEEPSwitchBranch` (or a sibling-load
  variant).
* `08-weave-marker-line-anchor/` — `be get file://<bare>?master`
  twice across a wt edit + an upstream commit that touch the same
  line.  Pre-fix `WEAVEEmitMerged` dropped `<<<<` / `||||` / `>>>>`
  at token boundaries — clusters held only the diverging fragment
  (`LOCAL` vs `UPSTREAM`) so neither side reconstructed a syntactic
  line when extracted.  Asserts the reframer runs (markers on their
  own lines) AND each cluster carries the full conflict line
  (`    if (cond && value > LOCAL) {` / `... > UPSTREAM) {`), so
  dropping one side leaves a buildable file.  Inline framing
  (cluster < 1/4 of the line) is preserved for tiny single-word
  edits via a separate path; this case exercises the line-level
  branch.
