# put/ — `be put` (stage) integration cases

* `01-states/` — three-file matrix (unchanged / modified / new) staged
  in one `be put *.txt` after a baseline commit; pins down how `be put`
  behaves when one of the listed paths is unchanged versus modified or
  new.
* `02-put-dir/` — `be put <dir>/` on a tracked subtree.
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
