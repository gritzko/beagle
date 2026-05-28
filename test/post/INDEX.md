# post/ — `be post` (commit) integration cases

* `01-bare-msg/` — put + post on a fresh worktree; smallest possible
  case (one file, one commit).  Does not exercise GET.
* `02-two-children-promote/` — two child branches each grow two commits
  and promote into trunk; first promote ff's, second triggers rebase.
* `03-rebase-on-divergent-parent/` — `be post ?..` from a child whose
  parent advanced; trunk + child cur auto-sync to the rebased tip.
* `04-criss-cross-merge/` — two siblings each `be patch` the other and
  post; a third cross-patch demonstrates dogs sidestepping the classic
  criss-cross via single-parent commits and PATCH provenance erasure.
* `07-patch-multi-author/` — two cherry-picks from a fix branch with
  two authors, then bare `be post`: the resulting commit must inherit
  message + author from the topologically latest patched commit, with
  ` (+N)` and ` (et al)` decorations for the count and author mix.
* `18-triangle/` — triangular `be post` (FF push) propagation across
  a 3-node ring (be↔be, be→git, git→be).  Three commit rounds
  (modify / add / delete) push around the triangle, plus a no-op
  empty rotation.  Surfaced the `keeper receive-pack` pack-drop
  bug (fixed in `keeper/RECV.c::RECVIngestPack`, locked in via
  `keeper/test/RECEIVEPACK.c::RECEIVEPACKtest_single_create`).
  Gated on `WITH_SSH`.  Companion: get/23, put/06.
* `09-sub-flat-both-dirty/` — `be post '#round1'` with outer + sub
  both dirty (SUBS.plan.md §POST): sub commits first via the BEPost
  wrapper's post-order recursion, parent's commit records the bumped
  gitlink via `SNIFFSubReadTip` + per-sub `be put <subpath>` staging.
  Asserts both wtlog tips advanced and the parent's new vendor/ tree
  references the sub's new sha.
* `08-sibling-ff-migrate/` — `be post ?<branch>` (no msg) FF-promotes
  a sibling/parent label to cur.tip and copies the missing commit/
  tree/blob objects from cur's shard into the target shard via
  `KEEPMoveCommits`.  Two-trunk-commit baseline + two siblings
  ping-pong commits via promotes; finally fix1 promotes its full
  stack to trunk.  Asserts target REFS advance, cur stays put,
  target shard's pack bytes grow on each promote, wt content
  matches on every switch, and the final trunk first-parent chain
  is intact.  After each switch also runs `be spot:.c#<sym>` for
  symbols whose blobs only exist on the migrated side, and
  `be log:#10` to confirm graf's commit-history index walks the
  full chain — all three shards (?fix1/?fix2/trunk) get the same
  index sanity sweep.
* `19-pure-push-no-commit/` — `be post //origin` (pure-push form)
  must NOT mint a commit on cur, even when a sub-mount triggers
  BEActSubsPost's gitlink-bump auto-`put`.  Sub fixture with a
  local sub commit + FF-push exercises the full recurse → bump →
  parent-sniff-post chain.  Passes today in the simple shape (the
  bump alone doesn't trip selective-mode commit creation); see
  test/TRIANGLE.todo.md §"BEActSubsPost…selective mode" for the
  conditions that did mint a commit in the originating trace.
  Gated on `WITH_SSH`.
* `20-force-flag-spec/` — `be post --force "msg"` is silently
  accepted (used internally to bypass the POSTCFLCT conflict-marker
  scan) but VERBS.md §POST defines no `--force`.  WILL_FAIL until
  the spec documents the override OR the dispatcher refuses the
  flag.  Per the originating report (item 5): "Either define it or
  refuse the flag at the dispatcher."
