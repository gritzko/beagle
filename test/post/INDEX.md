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
* `24-missing-branch-no-push/` — DIS-020: `be post //host?<branch>`
  where the target branch doesn't exist locally must ABORT before any
  wire push.  Previously `POSTPromote` returned `POSTNONE` (overloaded
  with "nothing to post"), whose low byte matches `NONE`, so `be`
  swallowed it as no-op-OK and proceeded to `BEActKeeperPush` (doomed
  WIRECLNFF).  Now the missing-branch sites return the distinct
  `NOBRANCH` (`sniff/SNIFF.h`); its low byte differs from `NONE`, so
  the plan runner aborts.  Local-only (no ssh): asserts non-zero exit,
  `NOBRANCH` on stderr, and that the keeper push stage never fired.
* `25-dot-branch-no-git-wire/` — DIS-019: a submodule's current branch
  is a be-only synthetic dot-coordinate (`?/<sub>/.<parent>`); posting
  that worktree DIRECTLY to a git remote forged `refs/heads/.<x>/`
  ("funny ref") after a full pack build.  `keeper_post` now skips the
  wire (no pack) when the branch is a dot-coordinate and the resolved
  remote is a git transport (`DOGIsGitTransport`).  Fully offline
  (file://): asserts exit 0, no pack built, no funny ref, the bare
  repo's refs byte-identical.
* `29-sub-clean-nested-remote-push/` — SUBS-006: `be post
  ssh://host/<parent>.git?master` (git peer) recurses the local-commit
  step into mounted subs.  A CLEAN, detached nested sub forwarded
  `be post -q ?/<sub>/.<parent>/master` (no `#msg`, no patch rows),
  which routed to `POSTPromote` and refused the not-yet-materialised
  synthetic branch with `NOBRANCH` ("does not exist"), aborting the
  whole push (exit 157).  `POSTPromote` now returns `POSTNONE` (no-op)
  when the missing target is reached from a DETACHED wt (always a clean
  sub — a dirty one routes through `POSTCommit`, which auto-creates the
  branch at the pin); an attached wt still refuses `NOBRANCH` per
  DIS-020 (see `24-missing-branch-no-push`).  Depth-3 forest (parent →
  vendor/sub → vendor/leaf), only the parent edited: asserts the push
  exits 0, no NOBRANCH/abort on stderr, the parent git origin advanced,
  and both clean sub tips stayed put.
* `30-push-refused-reason/` — DIS-027: a refused `be post //remote` must
  SURFACE the peer's own reason, not collapse to an opaque `WIRECLFL`.
  A bare git origin installs a `pre-receive` hook that prints a known
  marker to stderr and exits 1; the wt makes a clean fast-forward commit
  (so the refusal is the peer's, not the client-side non-FF gate) and
  pushes.  Asserts non-zero exit AND that be's stderr carries both the
  report-status `ng` reason (`pre-receive hook declined`, the in-band
  signal be parses — previously only `trace()`'d) and the hook marker
  (relayed over side-band-2).  Fix: `wpush_send_update` advertises
  `side-band-64k`, `wpush_drain_status` demuxes band-2/3 → stderr and
  re-parses band-1 report lines, and `wpush_classify_report` prints the
  `ng`/`unpack` reason (`keeper/WIRECLI.c`).  Gated on `WITH_SSH`.
* `31-remote-branch-push/` — DIS-026: `be post ssh://peer?<branch>` must
  push cur's tip onto the peer's `refs/heads/<branch>` (FF), reading
  `?<branch>` as the REMOTE target — not a local label (was `NOBRANCH`).
  Positive: a full ssh:// URL `?master` advances the bare peer's master
  to cur, exit 0.  Negative: a no-authority `?nonexistent` still refuses
  `NOBRANCH` (DIS-020 preserved — the fix only skips full-transport URIs
  with a scheme; bare `//alias?branch` is unchanged).  Fix:
  `sniff/SNIFF.exe.c` skips a transport-scheme URI in local label-target
  selection so the wire refname reaches `keeper post`.  Gated on `WITH_SSH`.
* `32-push-file-local-store/` — POST-008: `be post` must push to a
  HOST-LESS local beagle store (`file:///abs/path`), not just a
  host-bearing ssh/be:// URI.  `keeper_post`'s old `u8csEmpty(g->host)`
  gate rejected `file://` (no authority/host) with `keeper: post needs a
  remote URI (ssh://...)` / `KEEPFAIL`, so pushing to a LOCAL store was
  unreachable even though fetch over the same edge worked.  Drives the
  LOCAL-EXEC keeper edge (`file://` → `keeper receive-pack` spawned
  directly), so it needs NO ssh and runs in the default suite (unlike
  post/22's `be://localhost` ssh hop).  Clones A→B via `file://`, B
  commits + FF-pushes back; asserts the push reports success, A's trunk
  ff-advances to B's tip, and A's wt followed.  Fix: `keep_post`'s gate
  now accepts any routable target (host OR authority OR path OR
  `?/<proj>` OR a transport scheme), mirroring fetch (`keeper/KEEP.exe.c`).
