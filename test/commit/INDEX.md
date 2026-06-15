# commit/ — `commit:` commit-object projector cases

`commit:?<ref|sha>` renders a commit object like `git show`: the commit
metadata (headers + message, subject bold) is immediately followed by
the commit's FULL unified diff, INLINE (COMMIT-002). Keeper owns
`commit:`, graf owns `diff:` (separate dog binaries), so keeper does NOT
re-roll the diff — the `be` dispatcher resolves `?<ref>` to a sha ONCE,
runs keeper `commit:?<sha>` for the metadata hunk, then runs graf's
`diff:?<sha>` (commit-show) and RELAYS its hunk stream after it (the
capture-and-replay the sub recursion uses). The diff's own sub fan-out
composes exactly like a bare `diff:?<sha>`. This supersedes COMMIT-001's
navigable `diff:?<sha>` link.

* `01-diff-link/` — metadata is intact and the diff is inlined (the
  changed token rides in the same stream, no separate link); the blame
  link form `commit:?<8hex>#<message>` resolves to the same commit (hex
  query authoritative, fragment is a human label).
* `02-inline-diff/` — COMMIT-002.  Two commits of `foo.c`; asserts the
  metadata header AND the inline diff body appear in one stream in every
  render mode (Plain / Color / TLV).
* `03-sub-inline-diff/` — COMMIT-002 sub composition.  A parent commit
  that bumps a submodule pin: `commit:?<sha>` shows the metadata, the
  inline gitlink range line, and the sub's pin-range content diff
  relayed path-prefixed under the subpath (route #3, local `file:`).
