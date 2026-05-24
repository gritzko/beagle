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
  branches from colliding on file_id.
* `09-refuse-no-commit/` — atomicity invariant, negative half.
  GETCheckout now appends the `get` ULOG row and advances local
  REFS BEFORE WALKTreeLazy mutates the wt; pre-flight refusals
  must therefore run BEFORE the commit point, or a rejected GET
  would leave a stray baseline row.  Dirties trunk's wt and tries
  a cross-branch `be get ?feat` — must refuse SNIFFDRTY and leave
  `.be/wtlog` row count unchanged + the dirty wt content untouched.
* `10-partial-recovery/` — atomicity invariant, positive half.
  T0 has files in a locked subdir + outside it; T1 modifies both.
  Get back to T0, chmod 555 on the subdir + chmod 444 on the file
  inside, then attempt `be get ?feat`.  WALKTreeLazy fails on the
  read-only path; the `get` row + REFS advance fired BEFORE the
  mutation, so `.be/wtlog` must grow even though the wt is partial.
  A follow-up `be get --force ?feat` (chmod restored) completes the
  materialisation idempotently — the documented recovery path for
  a baseline-correct / wt-mid-flux state.
* `20-sub-already-mounted/` — second `be get $PARENT_URL?master` on a
  parent whose submodule is already mounted at the requested pin must
  skip the upstream fetch.  Regression for "be get hangs on submod
  update": `SNIFFSubMount` used to call `WIREFetchAll` unconditionally
  on every parent get, even when the sub wt was already at pin.  Test
  moves `$SUB_BARE` aside between the two gets so any sub fetch would
  fail fast; with the fix the second get short-circuits and succeeds.
  Gated on `WITH_SSH`.
* `19-be-url-project/` — `be get be://host?/project/branch`:
  project segment comes from the FIRST path component of the
  query, not from the URL-path basename (the rule for non-be
  schemes).  Asserts `be_ensure_project_repo` lays down
  `.be/<project>/{refs,wtlog}` plus a `.be/wtlog` row-0 anchor
  pinning the wt to that shard.  Wire intentionally fails
  (nonexistent `.invalid` host); only the on-disk layout is
  checked because `be_ensure_project_repo` runs BEFORE the keeper
  fetch step.
