# blame/ — `blame:` projector cases

Path-scoped view projectors (`blame:`, and the other path-bearing
projectors) make ONE pager-vs-direct decision per projection, regardless
of which shard owns the path.  A projection whose path lands inside a
mounted submodule resolves in the SUB shard (the parent tree holds only a
`160000` gitlink there) yet must funnel through the SAME producer→pager
pipeline as a parent-repo file.  See BE-004 and the [Submodules] model.

* `01-sub-path-pager/` — BE-004.  `be --color blame:<sub>/<file>` must
  take the SAME bro-pager route as `be --color blame:<parent-file>`,
  not dump raw ANSI to bare stdout.  Pre-fix `BEProjector` ran
  `BEProjectorRouteToMount` and `return`ed BEFORE the bro-pager block,
  so a sub-path projection re-emitted the captured sub TLV in the
  parent's `HUNKOutColor` mode straight to the terminal.  Fixed by
  folding the mount relay into a bro pipe on a TTY (BERouteMountPaged,
  BE.cli.c).  The case forces `HUNKOutColor` with `--color` (hermetic:
  bro one-shot-dumps on a non-tty stdout, no interactive hang) and
  asserts the sub-path color header is pager-padded to the same width as
  the parent file's; `--plain` / `--tlv` stay the raw relay (no fold).

[Submodules]: ../../../replicated.wiki/wiki/Submodules.mkd "submodule model"
