# diff/ — `be get diff:<path>?<from>#<to>` projector cases

* `01-3revs/` — three commits of `foo.c`, each tagged via
  `be post -m vN '?tags/vN'`.  Captures the byte-exact unified-diff
  output for `tags/v1#tags/v2` and `tags/v2#tags/v3`.
* `02-divergent-children/` — trunk baseline, two child branches
  `?fix1` and `?fix2` (created via `be post ?./fixN`) each with one
  commit editing disjoint lines of `foo.c`.  Captures the byte-exact
  diff for `fix1..fix2` and the reverse `fix2..fix1`.
* `04-ins-after-block-ansi/` — repro for the ANSI render dropping an
  inserted line.  When token-LCS matches the new line's terminating
  `\n` as EQ on a different `\n`-segment, `bro_walk_hunk`'s in-pass
  loop skipped the segment with the IN bytes (its boundary `\n` was
  RM-side, hidden in IN-pass).  Plaintext (`HUNKu8sFeedLineBased`)
  was unaffected; only the ANSI path lost the line.
* `06-sub-change/` — SUBS-012.  `diff:` aggregates a mounted sub's
  working-tree change into the parent's hunk stream, path-prefixed
  under the mount (`vendor/sub/core.c`); `--nosub` suppresses the sub
  side.  Driven by `BEProjectorSubs` (BE.cli.c) fanning whole-tree
  projectors into mounts via `BERelaySub`.  (ssh fixture.)
* `07-hex-named-branch/` — URI-001 §"The one rule" (Stage 4c).  A
  clone of `02` with `fix2` renamed to the all-hex `c0ffee`; the
  byte-identical want files prove a branch whose NAME is hex resolves
  BRANCH-FIRST in a diff ref (`GRAFRefIsName`), never misrouted into
  keeper's hashlet (`#<sha>`) path.  RED before Stage 4c, GREEN after.
* `08-sub-pin-bump-gitlink/` — DIFF-001 part-a.  `be diff:?<sha>` on a
  commit whose only change is a submodule PIN BUMP renders the gitlink
  line `<sub> <old-pin>..<new-pin>` (`diffref_emit_gitlink`,
  `WALK_KIND_SUB` collection in `graf/DIFFREF.c`).  Local-git submodule
  fixture, no ssh.  (Part-b — recursing into the sub for its
  commit-range CONTENT diff — is still open; see DIFF-001.)
* `09-sub-dirty-pager/` — DIFF-002.  Working-tree `be diff` whose only
  change lives in a mounted sub's worktree must route the sub diff
  THROUGH the parent's pager (bro), not past it.  Forces the bro-pager
  branch with `--color` and asserts the sub hunk is BRO-rendered
  (`--- <sub>/<file>:<line> ---`), not relay-ANSI past bro
  (`diff:<sub>/<file>#L<line>` = the bug).  Deterministic — bro's
  one-shot non-interactive drain, no PTY.  Fix:
  `BERunPipeSubs` in `beagle/BE.cli.c` folds the sub fan-out into bro's
  single TLV stdin.  Local-git submodule fixture, no ssh.

## Label form note

The bare label form `?v1..v2` does **not** resolve when `vN` only
appears as a commit message — `be post v1` stamps the message,
not a ref.  For tag-style names refs must be created explicitly
with `be post -m <msg> '?tags/<name>'` (case `01-3revs/`).
*Branch* labels born via `be post ?./fixN`, however, ARE first-class
refs and resolve directly in `?fix1..fix2` (case `02-divergent-children/`).
