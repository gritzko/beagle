# tree/ — `tree:` / `blob:` object-projector cases

Object-resolving view projectors (`tree:`, `blob:`, and the other
path-bearing keeper projectors) whose path lands inside a mounted
submodule resolve in the SUB shard (the parent tree holds only a
`160000` gitlink there).  See SUBS-012 and the [Submodules] model.

* `01-sub-object-routing/` — SUBS-012.  `tree:vendor/sub?master`
  descends into the sub tree; `blob:vendor/sub/core.c?master` emits
  the sub blob's bytes.  Driven by `BEProjectorRouteToMount` (BE.cli.c)
  which rewrites the projector mount-relative and relays it into the
  sub (BERelaySub re-prefixes the hunk URI under the mount).  `--nosub`
  opts out (the path stays parent-bound and fails as before).

[Submodules]: ../../../replicated.wiki/wiki/Submodules.mkd "submodule model"
