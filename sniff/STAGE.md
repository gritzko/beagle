# sniff staging packs — RETIRED

This document described a per-branch `stage.be/wtlog` / `stage.idx` pair
and the repack-into-canonical dance that an earlier sniff used to
hold uncommitted tree and blob objects between `put`/`delete` and
`post`.  That model is gone.

In the current design:

* `put` / `delete` append one row to `<wt>/.be/wtlog` (the ULOG) and
  do no pack I/O at all.
* `post` walks the baseline tree (keeper for single-hash URI, graf
  for multi-hash merge URIs) together with the worktree, resolves
  the change-set from ULOG rows + mtime attribution, and emits one
  keeper pack `commit → trees → blobs` on the spot.  No staging
  intermediary, no repack.

See `sniff/INDEX.md` for the live architecture and `dog/ULOG.md` for
the ULOG row format.  `.be/wtlog` is now a plain file (the ULOG), not
a directory — references to `.be/wtlog/at.log` are historical.  This
file is kept only so old commits / PRs keep resolving; do not add
new content here.
