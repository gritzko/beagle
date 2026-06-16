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
* `25-dot-branch-no-git-wire/` — DIS-019 + POST-013: a submodule's
  current branch is a be-only synthetic dot-coordinate
  (`?/<sub>/.<parent>`); posting that worktree DIRECTLY to a git remote
  would forge `refs/heads/.<x>/` ("funny ref") after a full pack build.
  DIS-019 refused outright; POST-013 refines that — since the user named
  a usable git target, the push now DEFAULTS to the remote's OWN default
  branch (advertised symref HEAD, here `master`) instead of refusing.
  Fully offline (file://): asserts exit 0, no funny ref, no dot-branch
  forged on the remote, and the remote's default branch FF-advances to
  cur's tip.  Fix in `keeper/KEEP.exe.c::keeper_post` (synthetic-coord
  guard → `git_wire_default`) + `keeper/WIRECLI.c::WIREPush`
  (`to_default` → `wpush_default_refname`, HEAD→main→master→first head).
* `37-sub-synth-git-default-branch/` — POST-013: the synthetic-coordinate
  → git-wire branch-resolution decision, both halves against one
  `.gitmodules`-declared git remote.  DEFAULT: `be post <git-remote>`
  (no `?branch`) on a sub on `?/<sub>/.<parent>` resolves to the remote's
  advertised default branch `main` (distinct from post/25's `master`, so
  a hard-coded fallback would mis-resolve), without touching the
  non-default `other` branch.  EXPLICIT: `be post <git-remote>?other`
  still pushes to the named ref `other` (the POST-013 default does not
  override an explicit `?branch`).  Fully offline (file://).
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
* `38-sub-push-beagle-peer-shard/` — SUBS-017: a parent
  `be post file://<peer>?/<parent>` (beagle peer holding BOTH projects)
  must recurse the push into a mounted, detached sub so the sub's commit
  lands on the sub's OWN sibling shard `?/<subproj>` — not the peer's
  row-0 default (the parent's shard) under the synthetic dot-branch.
  Pre-fix the parent forwarded the bare locator (no `?/<subproj>`
  selector) AND built it `scheme + "://" + authority`, double-emitting
  the `//` for `file:///abs` (`file://///abs`); the sub push died
  `WIRECLFL` / exit 157.  Fully local (`file://`, no ssh): clones a git
  parent+sub chain into a beagle peer store, re-clones into a wt, edits
  the sub, recursive-commits, then pushes; asserts each project's shard
  advanced to its OWN local tip and the parent shard carries no
  `?/sub/.par` dot-branch.  Fix: `BEActSubsPost` / `bepush_sub_uri`
  compose the locator via `URIMake` and re-attach each sub's
  `?/<subproj>` selector, mirroring GET's `subs_candidate_from_source`
  (`beagle/BE.cli.c`).
* `37-push-central-store-no-home-escape/` — POST-014: a push to a CENTRAL
  (multi-project `~/.be`-style) store must NOT escape the recv-side
  colocated wt-advance up to the store PARENT (`$HOME`).  The recv derived
  the wt root as `dirname(dirname(<shard>))` (= the store parent) and
  accepted it as a primary wt whenever ANY `<parent>/.be/wtlog` existed —
  so a stray/legacy top-level wtlog made `be get ?` run with cwd=`$HOME`,
  materialising the push into the WRONG tree (or failing 157) while the
  real secondary worktree was stranded, and the child's raw `Error:
  SNIFFFAIL`/`BEDOGEXIT` made a SUCCESSFUL push read as a failure.  Builds
  a private scratch `$HOME` with a central store (shards `proj`+`other`, a
  stray top wtlog), a secondary wt `A` anchoring back (`.be` is a FILE,
  like /home/gritzko/beagle), and a peer `B` that FF-pushes; asserts exit
  0, the store ref advanced, NO stray `$HOME` checkout / wtlog growth, and
  NO masked-failure noise.  Fix: `keeper/RECV.c::RECVCaptureWtPath` only
  advances when `h->wt` (the home's own wt) equals the two-pop wt root —
  a central store fails that gate and is skipped; a deferred advance is
  reported honestly.  Drives the local-exec `file://` edge; no ssh.
* `37-msg-scheme-roundtrip/` — POST-007 (1): a commit message
  containing `be://` (or any `scheme://` / doubled `//`) must round-trip
  BYTE-IDENTICAL — the message is opaque free text, never path-
  normalized, so the doubled slash is preserved (`be://`, not `be:/`).
  Guards all three message forms (explicit `#`-led, whitespace-bypass,
  legacy `-m`); the underlying verbatim-fragment routing landed with
  POST-002 (`dog/DOG.c::DOGNormalizeArg`), this case is the e2e lock-in.
  No ssh; runs in the default suite.
* `42-refuse-atomic-store/` — POST-017: a REFUSED `be post` (POSTCFLCT
  conflict-marker scan) must be ATOMIC — the store reflog (`refs`),
  keeper pack + idx segments, and wtlog stay BYTE-IDENTICAL to before
  the attempt (no orphan objects, no fresh idx run, no stray `post`
  row).  Table-driven (inline / line-block triples refuse atomically;
  plain file commits; `--force` re-commits a marker file).  The fix
  hoists the conflict scan to a pre-flight (sniff/POST.c step 5a) before
  any pack open / object write / index.  `.refs.idx` / `.wtlog.idx` are
  LSM read-caches excluded from the byte check (a plain read churns
  them).  No ssh; runs in the default suite.
