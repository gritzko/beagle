# get/ — `be get` (checkout) integration cases

* `01-checkout/` — put + post + `be get '?'` on a fresh repo with one
  file (greet.txt = "hello world\n"); pins down post stderr shape and
  that GET succeeds against the just-committed branch tip.
* `02-single-file-overwrite/` — `be get file.c?feat` (https://replicated.wiki/html/wiki/GET.html §GET):
  fork ?feat with a different `lib.c`, switch back to trunk, then
  pull only `lib.c`'s feat-side blob into the wt.  Asserts the file
  bytes flip and `.be/wtlog` does NOT grow (no staging — no `get` row).
* `03-subtree-overlay/` — `be get src/?feat` (trailing slash, https://replicated.wiki/html/wiki/Verbs.html
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
* `23-triangle/` — triangular `be get` propagation across a 3-node
  ring with all three edge types: be↔be (`be://localhost/<A>?`),
  be→git (`git fetch --upload-pack="keeper upload-pack"
  ssh://localhost$ABS_B main:main` against the be peer), git→be
  (`be get ssh://localhost/<C.git>?` against a git bare).  Three
  commit rounds (modify / add new / delete) propagate around the
  triangle; an empty rotation at the end is a no-op + a final
  `be sha1:?` tip-equality check.  Gated on `WITH_SSH`.
* `24-uri-refs/` — `be get be://localhost/<U>?<form>` URI-ref
  resolution probe (https://replicated.wiki/html/wiki/URI.html §"Ref resolution"): all four query
  shapes — absolute `?/proj`, absolute-with-branch `?/proj/feat`,
  project-relative `?feat`, cur trunk `?`.  Currently `WILL_FAIL`
  — absolute trunk + `?` work; `?/proj/feat` and `?feat` still
  trip on `keeper upload-pack`'s single-shard serve path (see
  test/TRIANGLE.todo.md).  Gated on `WITH_SSH`.
* `19-be-url-project/` — `be get be://host?/project/branch`:
  project segment comes from the FIRST path component of the
  query, not from the URL-path basename (the rule for non-be
  schemes).  Asserts `be_ensure_project_repo` lays down
  `.be/<project>/{refs,wtlog}` plus a `.be/wtlog` row-0 anchor
  pinning the wt to that shard.  Wire intentionally fails
  (nonexistent `.invalid` host); only the on-disk layout is
  checked because `be_ensure_project_repo` runs BEFORE the keeper
  fetch step.
* `25-remote-shard/` — `be get be://host?/project/branch` lays
  down the per-host remote shard `.be/<project>/remotes/<host>/refs`
  parallel to the project shard.  https://replicated.wiki/html/wiki/Store.html §"Repo dir layout":
  remotes live in a `remotes/` class dir next to branches, one
  subdir per host; cache-only (no wtlog inside).  Same
  wire-intentionally-fails shape as 19; asserts the dispatcher-
  side mkdir + idempotency on re-invocation.
* `26-cached-no-wire/` — `be get //host?<ref>` (cached form, no
  transport scheme) must NOT open the wire.  WILL_FAIL today —
  `keeper/KEEP.exe.c::keeper_get` dispatches on
  `!u8csEmpty(g->authority)` and skips the `g->scheme` check; the
  matching `BE_PLAN_PATCH` row at `beagle/DISPATCH.c:199` correctly
  requires `URI_SCHEME|URI_AUTHORITY`.  Repro clones over ssh,
  moves the origin tree aside, then issues the cached form — under
  the bug the wire dies on the missing path.  Gated on `WITH_SSH`.
* `27-ff-refuse/` — `be get` against a linear-ahead local cur
  (server tip is local's parent) must refuse (FF rule on the local
  branch tip, https://replicated.wiki/html/wiki/GET.html §GET).  Regression guard: today the gate at
  `sniff/GET.c::~1131` fires correctly for `ssh://origin?master`,
  `ssh://origin`, and `//localhost`.  The sub-mount path may bypass
  it (originating `be get //spot` trace) — that gap is tracked
  separately in `test/TRIANGLE.todo.md`.  Gated on `WITH_SSH`.
* `28-linear-ahead-preserved/` — three-shape sweep around Bug 6:
  with cur ahead of origin by one local commit, every `be get`
  shape (`ssh://origin?master`, `ssh://origin`, `//origin`) must
  leave the local commit reachable — either by refusing the GET or
  by parking the displaced tip on a recoverable ref.  Originating
  trace: `f922149d4c44e987…` was dropped quietly by `be get //spot`
  on a sub-mount setup.  Passes today in flat-repo shape; the
  sub-mount repro stays open.  Gated on `WITH_SSH`.
* `32-sub-same-project-twice/` — the SAME sub project mounted twice by
  one parent, each gitlink pinned to a different branch/commit
  (`first`@A on `master`, `second`@B on `other`).  Both mounts share a
  single store shard (`.be/sub/`, keyed by url-basename) yet each
  checks out its own pin; asserts the two mounts hold distinct bytes
  (alpha vs beta), the recorded pins differ, and no private
  `.be/first`/`.be/second` shard was spun up.  Gated on `WITH_SSH`.
* `29-file-worktree/` — `be get file:<beagle-store>` wires a local
  sibling worktree: cwd's `.be` becomes a regular wtlog file pointing
  back at the shared store, tip tree checked out (https://replicated.wiki/html/wiki/Verbs.html
  §"Worktree management" Example 2).  Local-only, no ssh.
* `30-file-git-clone/` — `be get file:<git-repo>` clones a LOCAL git
  repo via a locally-spawned `git-upload-pack` (same transport flow as
  `ssh://`), exercising the single-slash `file:<abs>` form.  Companion
  to get/29: same URI form, on-disk-type routing (store → worktree,
  git repo → clone).  Local-only, no ssh.
* `34-restore-deleted-tracked/` — DIS-017: a bareword `be get <file>`
  classifies path-vs-branch by TRACKED-in-baseline-tree status, not
  on-disk stat. Asserts (A) a deleted tracked file is resurrected from
  cur's baseline, (B) a present-but-edited tracked file is restored,
  (C) a non-tracked bareword still routes to `?branch` (real branch
  switches, bogus name refuses; an untracked on-disk file never hijacks
  the path slot). Local-only, no ssh.
* `38-remote-main-branch/` — DIS-028: an explicit `?main` against a git
  remote resolves the peer's literal `refs/heads/main`, like `?master`
  already does, instead of failing `WIRECLFL` / `the remote end hung up
  unexpectedly` (exit 157).  The trunk⇔`refs/heads/main` wire alias used
  to fold the ADVERTISED `main` to be-side empty in the ref matcher, so
  a literal `?main` want never matched its own ref.  Bare git peer with
  `refs/heads/main` AND `refs/heads/master` at one tip; asserts `?master`
  resolves (contrast), `?main` resolves (the fix), and bare-trunk (no
  `?ref`) still maps to `refs/heads/main`.  Gated on `WITH_SSH`.
* `39-be-pathless-project/` — GET-003: the keeper clone form with the
  project in the QUERY and an EMPTY path (`be get be://host?/proj`,
  `be head be://host?/proj`) — empty path = the peer's default store,
  `?/<project>` selects the shard.  Pre-fix `wcli_spawn`
  (keeper/WIRECLI.c) rejected an empty path with `WIRECLFL` BEFORE
  scheme classification, so the pathless keeper form died exit 157.
  Fix: the empty-path rejection is now scheme-specific — keeper peers
  with a `?/<project>` query accept an empty path (serve `?/proj`);
  git transports (ssh/https/git) still require a repo path.  Hermetic
  via the LOCAL-EXEC keeper (`keeper://local`, no ssh): asserts
  `be head`/`be get keeper://local?/proj` resolve + clone, and the
  selector-less keeper empty path AND git-transport empty path both
  still `WIRECLFL`.  Local-only, no ssh.
* `40-no-ancestor-wtlog-escape/` — GET-006: `be get file:<store>` from a
  FRESH subdir (no `.be`) of someone else's worktree must not let store
  discovery walk UP and append a `get` row into the ANCESTOR store's
  `.be/wtlog` (self-anchoring / poisoning it).  Fix
  (`BEActWorktreeAnchor`): after `BEGetWorktree` wires `<cwd>/.be`,
  re-target `c->repo` + the `--at` URI at cwd so the downstream get
  opens THIS worktree, never the ancestor.  Hermetic — the fake
  ancestor store lives inside `$SCRATCH` so the walk-up can never reach
  the real `$HOME/.be` (also asserted byte-identical).  Local-only.
* `42-empty-source-refuse/` — GET-010: `be get "file://<store>.be?/proj"`
  from a SOURCE store whose `refs` is empty/elided must refuse cleanly,
  never manufacture store damage on the source side.  Pre-fix the keeper
  upload-pack SERVER (opened read-only to serve the clone) resolved the
  `.be`-suffixed source path as a worktree ROOT, re-appended `.be`
  (→ `<store>/.be/.be/<proj>`), then `KEEPOpenBranch` `FILEMakeDirP`'d
  that doubled trunk dir even on a READ — a stray nested shard with a
  fresh 0-byte `refs` + `.refs.idx` written into someone else's store on
  a mere read, while the clone STILL failed.  Fix
  (`keeper/KEEP.c::KEEPOpenBranch`): a read-only keeper open never
  creates directories (the trunk-dir mkdir is now `if (rw)`); the
  pack/idx scan already tolerates an absent dir, so RO reads zero refs
  and the clone fails CLEANLY (`WIRECLFL`) with no stray shard / files.
  Hermetic; asserts the source `.be` tree is byte-identical after.
  Local-only.
* `44-file-subs-parent-no-getrow/` — GET-011: a `be get` against a
  beagle store whose parent shard's recorded `get` row is NOT a
  recoverable beagle remote (an unreachable upstream — the real
  `~/.be/<proj>` shape) must still source each submodule from the live
  source store, with the declared `.gitmodules` URL only last.  The sub
  objects sit in a SIBLING shard named by the beagle PROJECT (`ch`) while
  the declared URL's basename differs (`dead-libch`, an UNREACHABLE
  github-style URL) — so the url-basename sub-shard misses the pin.
  Pre-fix, sniff built no parent-source candidate and tried only the
  offline URL → `WIRECLFL`/`BEDOGEXIT`.  Fix (`sniff/SUBS.c`):
  `subs_recover_locator` falls back to the live parent STORE root when no
  beagle `get` row is recoverable, and `SNIFFSubMount` prefers the
  path-basename sibling shard when it already holds the pin (mounts
  locally, no wire self-fetch).  Hermetic; asserts the sub materialises
  and the declared URL is never the landing source.  Local-only, no ssh.
* `45-file-subs-inflight-source/` — GET-011 (primary mechanism): when
  `be get` clones from a beagle remote (`file://<store>?/<proj>`), each
  submodule is fetched from THE EXACT SOURCE WE ARE TALKING TO — the
  in-flight `be get` source URI on the command line — as the PRIMARY
  candidate, with the declared `.gitmodules` URL only the LAST fallback.
  Cross-store: source store B1 holds sibling shards `par`+`ch` (sub
  objects in the path-basename `ch` shard), the committed `.gitmodules`
  URL is UNREACHABLE with a differing basename (`dead-libch` != `ch`,
  the dogs `libabc.git` vs `abc` shape), and we clone `file://B1?/par`
  into a FRESH dest B2 so the sub fetch is a genuine wire round-trip
  (not a same-store local sibling swap).  The fix threads the in-flight
  source down beagle → `sniff sub-mount --source` → `SNIFFSubMount`,
  which builds the PRIMARY candidate `file://B1?/ch` (source
  scheme+authority+store-path, `?/<proj>` swapped to the sub's
  path-basename).  Asserts `B2/ch/c.txt` materialises byte-exact, the
  in-flight `?/ch` candidate is the landing source, and the dead URL is
  never fetched.  Local-only, no ssh.
* `49-force-full-reset/` — GET-016 Part 2: `be get --force` (`get!`) must
  converge a DRIFTED wt to the target even when the baseline↔target delta
  is empty.  Seeds trunk `{a, sub/b}`, deletes `sub/b` on `?feat`, then
  drifts: advances the ref to `?feat` with `sub/` read-only so the unlink
  of `sub/b.txt` is lost — the orphan now sits in NEITHER baseline nor
  target tree, so plain GET never revisits it.  `be get --force '?feat'`
  must unlink the wt-only TRACKED orphan (mtime stamps a prior get/post
  row) while PRESERVING an untracked clutter file (only `--prune` removes
  those — GET.mkd §Flags), leaving `be status` free of phantom mod/mis.
  Hermetic.
* `50-divergent-no-trunk-clobber/` — GET-014: a read/merge verb against
  a DIVERGENT peer (shared 3-commit base B, peer tip C, local trunk T,
  C↔T non-reachable) must NEVER silently move the local trunk `?` to C.
  Two independent file:// stores; runs `be head`, `be get`, `be patch`
  from a fresh wt at T against the divergent peer and asserts the parent
  store's trunk `?` tip stays at T (the original bug dropped four landed
  commits when `.refs.idx` regen mispicked C — root-caused to DIS-038's
  ancestor-fallback gap, now landed).  Also asserts `be head` (the
  read-only GET dry-run) appends no bare-`?` trunk `get` row (a peer-uri-
  keyed remote-tracking observation is the allowed cache refresh).
  Hermetic.
* `51-ff-weave-unindexed-merge/` — DIS-041: a behind-worktree FF
  (`be get '?'`) on a file BOTH sides edited (disjoint regions) must
  weave the union of both edits even when the trunk commits live in
  keeper packs but NOT in graf's persisted DAG index (the state a wire
  push / `be patch` / fresh clone leaves).  Bare central store, 5 trunk
  commits each editing a distinct top line, behind-wt dirty-edits a
  bottom line; the store's `*.graf.idx` is wiped before the FF.  Without
  the fix DAGTopoSortTunable saw every commit as a parent-less root,
  hashlet-sorted them, and the replay dropped intermediate edits
  (non-deterministically) — never `graf err`, just silent loss.  The fix
  (graf/GET.c::GRAFMergeWtFileTunable) indexes both merge endpoints into
  graf BEFORE the weave so the topo replay is coherent; asserts all five
  trunk edits AND the wt edit survive, no `graf err` / `untouched`.
  Hermetic.
