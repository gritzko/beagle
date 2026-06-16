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
* `08-sub-pin-bump-gitlink/` — DIFF-001 parts a + b.  Part-a: `be
  diff:?<sha>` on a commit whose only change is a submodule PIN BUMP
  renders the gitlink line `<sub> <old-pin>..<new-pin>`
  (`diffref_emit_gitlink`, `WALK_KIND_SUB` collection in
  `graf/DIFFREF.c`).  Part-b (option B, be-driven relay): the gitlink
  line is FOLLOWED by the sub's actual content diff for the pin range
  (`chsub/c.txt: v1 -> v2`), path-prefixed under the mount; `--nosub`
  keeps the gitlink line but drops the sub content.  `be` parses the
  gitlink pin pair out of graf's diff report (`bediff_parse_gitlinks`,
  route #1) and rewrites each bumped sub's child URI to
  `diff:?<old>#<new>` in the projector fan-out (`BEProjectorSubsPins` /
  `beproj_recurse_cb`, `beagle/BE.cli.c`).  Local-git submodule
  fixture, no ssh.  Also DIFF-002 (the channel fix): the pin-bump diff
  must travel THROUGH the pager (bro), proven via the same `--color`
  one-shot bro drain diff/09 uses (`v2` content + the BRO-002
  `48;5;230m … chsub/c.txt` banner in bro's own output).  Pre-fix the
  gitlink line was a raw `fprintf(stdout)` that, in `--tlv` mode, led
  the stream UN-enveloped, so `bro_drain_tlv`→`HUNKu8sDrain` failed the
  `HUNK_TLV` gate at offset 0 and folded the WHOLE stream to zero hunks
  ("be: no results").  Fix: `diffref_emit_gitlink` (`graf/DIFFREF.c`)
  emits the line through `GRAFHunkEmit` (a text-only hunk) so every mode
  shares the one `HUNKu8sFeedOut` channel.
* `11-file-scope-full/` — DIFF-003.  A file-scoped `diff:<file>#L<n>`
  (the `U` hunk nav-URI target) renders the WHOLE file with the changed
  lines highlighted in place, while a tree/dir-scoped `diff:` stays
  changed-hunks-only.  A 30-line file with one change at line 20; the
  far unchanged line (`line 01 original`, 19 lines from the change, well
  past the 3-line context window) MUST appear in the file-scope view
  (Plain/Color/TLV) and MUST NOT appear in the tree-scope view.  Fix:
  graf keys the emit scope off file-vs-tree (`GRAFDiff2Layer(...,full)`),
  routing file scope through `WEAVEEmitFull` (every line, change-tagged,
  `diff:` scheme so the renderer keeps the +/- formatter) and tree scope
  through `WEAVEEmitDiff` (windowed); bro owns the highlighting.  No
  fixture, no ssh.
* `12-difflet-click/` — DIFF-004.  A diff hunk's per-hunk `U` click
  target must carry the range in the QUERY as `diff:<path>?<from>..<to>`
  with the fragment kept as the `#L<n>` line anchor, so clicking re-opens
  the file-scoped whole-file RANGE diff (DIFF-003) — NOT the empty-query
  `diff:<path>#L` form (which runs wt-vs-base, empty for a committed
  file → "no results").  Two-commit 30-line file: (a)
  `diff:<path>?<from>..<to>#L<n>` resolves to the whole-file range diff
  (far unchanged line + change) in Plain/TLV; (b) a commit-show diff's
  per-file hunk nav URI is the `?from..to#L` form, not empty-query; (c) a
  bare `?..` parent-branch ref is NOT mis-split — its nav URI stays the
  bare `#L` form.  Fix: graf parses `?from..to` (reusing the `weave`
  verb's `..` split via `graf_query_range`, guarded on non-empty halves)
  and threads a `navver` (`<from>..<to>`) through
  `GRAFDiffTreeRefs`/`GRAFWeaveDiff` → `GRAFDiff2Layer` →
  `WEAVEEmitDiff`/`Full`, spliced into each hunk URI's query.  No
  fixture, no ssh.
* `09-sub-dirty-pager/` — DIFF-002.  Working-tree `be diff` whose only
  change lives in a mounted sub's worktree must route the sub diff
  THROUGH the parent's pager (bro), not past it.  Forces the bro-pager
  branch with `--color` and asserts the sub hunk is BRO-rendered
  (`--- <sub>/<file>:<line> ---`), not relay-ANSI past bro
  (`diff:<sub>/<file>#L<line>` = the bug).  Deterministic — bro's
  one-shot non-interactive drain, no PTY.  Fix:
  `BERunPipeSubs` in `beagle/BE.cli.c` folds the sub fan-out into bro's
  single TLV stdin.  Local-git submodule fixture, no ssh.
* `14-color-adds-render/` — BRO-004.  `be --color diff:` must render the
  ADDED (`+`/IN) side.  A long inserted line replacing several deleted
  lines (shared trailing token suffix) makes the IN line's `\n`
  token-LCS-match onto the RM side; `bro_walk_hunk` accumulated the IN
  bytes past the block's last in-pass-visible `\n` and never flushed a
  row for them, so the addition vanished from the colour render (plain
  unaffected).  Fix: flush a trailing in-pass (and symmetric rm-pass)
  row at block end for any pending bytes.  Also asserts a small in-place
  word edit stays INLINE (one row, per-token tinting), preserving the
  line-OR-token-by-volume design.  Local, no ssh.
* `15-color-emdash-coherent/` — BRO-004 Part 2.  `be --color diff:` of a
  large multi-line→one-line replace sharing an em-dash tail must render
  the FIRST hunk COHERENTLY.  Token-LCS makes the shared tail EQ and
  lands the inserted line's `\n` on the RM side; `bro_walk_hunk` ended
  the modified block at that non-eq boundary, so the eq tail rendered
  TWICE (swallowed into the in-pass row via a hidden-`\n` overrun AND as
  a standalone NORMAL row, interleaved old/new, rm side missing its
  tail).  Fix: extend the block across a non-eq boundary `\n` so the eq
  tail renders once per surviving side, matching `--plain`.  Asserts on
  per-row structure (inserted line contiguous; standalone tail precedes
  it), not ANSI-stripped-vs-plain.  Local, no ssh.

## Label form note

The bare label form `?v1..v2` does **not** resolve when `vN` only
appears as a commit message — `be post v1` stamps the message,
not a ref.  For tag-style names refs must be created explicitly
with `be post -m <msg> '?tags/<name>'` (case `01-3revs/`).
*Branch* labels born via `be post ?./fixN`, however, ARE first-class
refs and resolve directly in `?fix1..fix2` (case `02-divergent-children/`).
