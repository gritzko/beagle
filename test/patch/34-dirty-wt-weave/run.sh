#!/bin/sh
#  34-dirty-wt-weave — PATCH-002: `be patch` MUST weave an incoming
#  branch's change into a worktree-dirty file, not silently skip it.
#
#  When a file is BOTH locally-modified in the wt (uncommitted) AND
#  changed on the incoming branch, the committed tree-sha triple is
#  {base, base, theirs} (committed `ours` == base, since the edit is
#  uncommitted).  The old code short-circuited that to `take-theirs`
#  / `noop` on the tree shas alone and overwrote / dropped the wt
#  bytes — silent data loss.  A following `be post` then reverted the
#  incoming side.  The fix probes the wt's on-disk bytes: a dirty path
#  whose incoming side changed enters the 3-way weave (base, theirs,
#  dirty-wt) → `merged` on disjoint regions, `content-conflict` on a
#  true overlap, never a silent skip.
#
#  Topology (fork B at base; cur stays on trunk):
#
#      base ── (cur, trunk, then dirty-edited in the wt)
#        \
#         B  (edits lib.c top region + k.txt K2)
#
#  lib.c: B edits `add` (top), the wt dirties `main` (bottom) — DISJOINT
#         → must weave clean (`merged`), keeping BOTH edits.
#  k.txt: B edits K2, the wt dirties the SAME line K2 — OVERLAP
#         → must report `content-conflict`, leaving token markers.

. "$(dirname "$0")/../../lib/case.sh"

OUT="$SCRATCH/../out"
mkdir -p "$OUT"

# base on trunk: lib.c + k.txt
sleep 0.02; cp "$CASE/01.lib.base.c"  lib.c
sleep 0.02; cp "$CASE/05.k.base.txt"  k.txt
"$BE" put lib.c k.txt >/dev/null
"$BE" post 'base' >/dev/null

# fork ?B at base (cur stays on trunk)
"$BE" put '?./B' >/dev/null

# switch to B, edit lib.c top + k.txt K2, commit
"$BE" get '?B' >/dev/null
sleep 0.02; cp "$CASE/02.lib.B.c"  lib.c
sleep 0.02; cp "$CASE/06.k.B.txt"  k.txt
"$BE" put lib.c k.txt >/dev/null
"$BE" post 'B edits add + K2' >/dev/null

# back to trunk (at base); DIRTY-edit lib.c bottom + k.txt K2, DO NOT commit
"$BE" get '?..' >/dev/null
match "$CASE/01.lib.base.c" lib.c     # trunk is clean at base before edits
sleep 0.02; cp "$CASE/03.lib.wt.c"  lib.c
sleep 0.02; cp "$CASE/07.k.wt.txt"  k.txt

# THE TEST: patch B's whole branch into the dirty wt.
"$BE" patch '?B!' >"$OUT/patch.out" 2>"$OUT/patch.err"

# lib.c (disjoint) must carry BOTH B's top edit and the wt's bottom edit.
match "$CASE/04.lib.want.c" lib.c

# It must be counted `merged` (≥1), NOT silently skipped as noop.
grep -q 'merged=[1-9]' "$OUT/patch.out" || {
    echo "FAIL: dirty-wt file not woven (merged=0) — PATCH-002 regression" >&2
    cat "$OUT/patch.err" >&2
    exit 1
}

# k.txt (true overlap) must report a content-conflict with markers.
grep -q 'content-conflict=[1-9]' "$OUT/patch.out" || {
    echo "FAIL: overlapping dirty-wt edit did not report content-conflict" >&2
    cat "$OUT/patch.err" >&2
    exit 1
}
grep -q '<<<<' k.txt || {
    echo "FAIL: no conflict markers left in k.txt after overlap" >&2
    cat k.txt >&2
    exit 1
}

# The dirty lib.c file must NOT have been counted take-theirs (the bug's
# signature: incoming change overwrites / drops the wt edit wholesale).
if grep -q 'take-theirs=[1-9]' "$OUT/patch.out"; then
    echo "FAIL: a dirty-wt path was take-theirs'd instead of woven" >&2
    cat "$OUT/patch.err" >&2
    exit 1
fi

# Resolve the conflict in k.txt (keep both lines) so POST does not refuse
# on the marker scan, then commit and confirm the woven lib.c survives —
# the incoming `add` edit is NOT reverted by the following post.
printf 'K1\nK2-FROM-WT\nK2-FROM-B\nK3\n' > k.txt
"$BE" post 'land both' >"$OUT/post.out" 2>"$OUT/post.err"
match "$CASE/04.lib.want.c" lib.c

echo "=== patch/34-dirty-wt-weave: OK ==="
