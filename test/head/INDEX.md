# head/ — `be head` (peek / dry-run) integration cases

* `01-ahead-behind/` — `be head ?..` and `be head ?` on a diverged
  feature branch surface ahead/behind log lines.  WILL_FAIL until
  the cur-vs-ref diff-summary entry point lands (see VERBS.todo.md).
* `02-msg-search/` — `be head '#wonderful'` walks cur's first-parent
  chain and prints the first commit whose message body contains the
  fragment as a substring.  Covers match, miss, and read-only
  invariant (cur tip unchanged after the call).

Submodule recursion cases (SUBS.plan.md §HEAD).  All use the
`test/lib/submodules.sh` fixture (parent + sub via ssh://localhost,
gated by WITH_SSH) and assert pre-order visitation via
`be: head <relpath>` markers on stderr.  WILL_FAIL until the
beagle-side BERecurseVerb wrapper and BEHead wiring land.

* `03-sub-clean-flat/` — clone parent (auto-mounts vendor/sub);
  `be head ?` visits both projects in pre-order; neither wtlog
  grows (read-only invariant).
* `04-sub-dirty/` — edit one file inside vendor/sub; `be head ?`
  surfaces the dirty path in the sub's section (a `mod`/`dirty`
  marker, not just the bareword) while the outer stays clean.
  SUBS-007: assertion tightened from a bare `core.c` grep (which the
  committed pin-vs-trunk diff already prints) to require the dirty
  marker, so it actually exercises the dirty-state contract.
* `21-sub-dirty-reported/` — SUBS-007 diff pin: `be head ?` run on a
  clean tree vs after dirtying one sub file must produce DIFFERENT
  output, and the added bytes name the sub's dirty path with a
  mod/dirty marker.  Guards against HEAD's per-sub report being
  byte-identical regardless of working-tree state.
* `05-sub-parent-ahead/` — commit on the parent only; `be head ?master`
  reports the parent's local commit in its ahead list and does NOT
  leak it into the sub's section.
* `08-sub-declared-not-mounted/` — `.gitmodules` lists vendor/sub
  but the anchor + checkout are removed.  HEAD reports
  "declared, not mounted" for the path and continues (does not
  recurse into the absent mount, does not refuse).

TODO, blocked on fixture extensions:

* `06-sub-depth3` — three-level forest (outer → sub → leaf).  Needs
  `submodules.sh` extended for a grand-child mount.
* `07-sub-fetch-recursive` — `be head ssh://origin` fetches every
  project's remote.  Needs a helper to advance the bare upstreams
  after the initial seed so ahead/behind has something to surface.
