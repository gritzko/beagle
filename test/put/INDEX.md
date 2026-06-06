# put/ — `be put` (stage) integration cases

* `01-states/` — three-file matrix (unchanged / modified / new) staged
  in one `be put *.txt` after a baseline commit; pins down how `be put`
  behaves when one of the listed paths is unchanged versus modified or
  new.
* `02-put-dir/` — `be put <dir>/` on a tracked subtree.
* `11-sub-add-refusal/` — SUBS-009 repro.  `be put <subpath>` on a
  directory declared in `.gitmodules` (or already holding a child
  `.be`) but NOT yet mounted gives an actionable refusal that names the
  missing `sniff sub-mount` step, instead of the misleading bare "does
  not exist — skipped" / PUTNONE.  Probe B then follows the named flow
  (sub-mount → `be put` → POST) and asserts the parent commit records
  both the `160000` gitlink and the `.gitmodules` section — proving the
  refusal points at a flow that actually works.  Gated on `WITH_SSH`.
* `09-sub-only-dirty-exit/` — SUBS-004 repro.  Parent worktree left
  clean, only a mounted submodule's file edited; bare `be put` recurses,
  stages the sub blob, and MUST exit 0 (not 206/PUTNONE — the parent
  sniff's empty-stage NONE must not mask the successful sub stage).
  Also asserts the must-still-PUTNONE invariant: a truly-empty bare put
  (parent clean AND every sub clean, on a fresh clone) still surfaces
  PUTNONE.  Gated on `WITH_SSH` (submodules.sh fixture).
* `06-triangle/` — triangular `be put` propagation across a 3-node
  ring (be↔be, be→git, git→be).  Three FF rounds (modify / add /
  delete) plus a non-FF rewrite tail in which B rewinds cur's tip
  via `be put ?#<sha>` and propagates the rewound history around —
  POST would refuse `POSTNOFF`, PUT must accept.  Currently
  `WILL_FAIL` at R4: `be put ssh://...` doesn't actually force-push
  (TRIANGLE.todo.md gap #3b — `WIREPush` refuses non-FF the same
  way POST does).  R0-R3 pass since the receive-pack pack-drop fix.
  Gated on `WITH_SSH`.  Companion: get/23, post/18.
* `05-submodule-bump/` — `be put <subpath>` on a sub-mount: PUT reads
  the sub-wt's pinned tip from its `.be` anchor and stages one
  `put <subpath>#<40-hex>` row.  No dir walk, no per-file hashing.
  Covers MODULES.plan.md §"Phase 4 — PUT" (explicit-arg form).
* `05-cross-shard-set/` — `be put ?<branch>#<sha>` migrates the new
  tip's reachable closure into the target's shard (KEEPMoveCommits)
  with a shared-ancestry guard.  Covers: existing-target FP-mismatch
  refusal, new-target migration (pack bytes grow, wt resolves), and
  bogus-sha resolver refusal.
* `07-newbranch-no-migrate/` — `be put ?<newbranch>/#<sha>` where
  the new branch will live as a child of cur's shard MUST NOT migrate
  cur's first-parent chain into the new shard.  Object retrieval
  already walks child → parent → root (keeper/INDEX.md §"Storage
  layout"), so the child reads cur's pack for free.  WILL_FAIL today
  — `sniff/PUT.c::PUTSetBranch` always calls KEEPMoveCommits on a new
  ref; on long histories the originating `be put ?recover/#<sha>`
  trace hung outright.  Asserts: new shard dir is created but holds
  zero `.keeper` packs; switching into the new branch reads the
  N-th-commit content via parent-shard fallback.
* `03-branch-shard/` — `be put ?<branch>` creates the branch's keeper
  shard dir (`.be/<branch>/`), and any subsequent `be post` on that
  branch writes its pack log INTO that shard (`.be/<branch>/NNNN.keeper`)
  with a `.keeper.idx` sidecar in the same dir.  Trunk's pack inventory
  must not change across the branch create + branch commit.
  Exercises the PAST/DATA partition on `keeper.packs`/`puppies` (see
  KEEP.h §"Branch-aware object store" and abc/Bx.h §PastDataS).
  WILL_FAIL today — the keeper-side partition is in place but sniff
  still opens keeper on trunk because branch-aware open breaks
  cross-sibling reads (`be get ?other` from `?cur`).  Resolving needs
  a recursive `.be/*` scan or per-op branch context.
