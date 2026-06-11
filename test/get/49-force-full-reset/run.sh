#!/bin/sh
#  49-force-full-reset — GET-016 Part 2.
#
#  `be get --force` (≡ `be get!`) must drive the worktree to EXACTLY the
#  target tree even when the baseline↔target delta is empty (a drifted
#  wt: the ref already advanced but the on-disk files never followed).
#  The hard half this case pins is the DELETION side: a tracked path
#  that the target tree no longer carries, yet still sits on disk
#  because a prior checkout's unlink was lost (here: blocked by a
#  read-only parent dir).  Once the baseline advances past it, the path
#  is absent from BOTH baseline and target, so the baseline↔target
#  delta never schedules its unlink — plain `be get` AND `be get
#  --force` both leave it forever (the "wt-only tracked orphan" bug).
#
#  --force must unlink such a tracked orphan (its mtime stamps a prior
#  get/post row, so sniff knows it wrote the file) WITHOUT touching a
#  genuinely-untracked file (which only `--prune` removes — GET.mkd
#  §Flags: "untracked files survive unless pruned").  Afterwards the wt
#  must match the target byte-for-byte and `be status` must be clean.
#
#  HERMETIC: case.sh's rs_fresh_wt seeds the per-wt empty-`.be/` shield
#  + the scratch-base firewall, so discovery can never escape to the
#  dev box's real $HOME/.be.

. "$(dirname "$0")/../../lib/case.sh"

# ---------------------------------------------------------------------
# 1. Seed trunk c0 with a.txt (kept) + sub/b.txt (later removed).
# ---------------------------------------------------------------------
mkdir sub
sleep 0.02; echo A > a.txt
sleep 0.02; echo B > sub/b.txt
"$BE" put a.txt sub/b.txt > /dev/null 2> 1.put.err  || { cat 1.put.err  >&2; echo "FAIL: put c0" >&2; exit 1; }
sleep 0.02
"$BE" post '#c0'          > /dev/null 2> 1.post.err || { cat 1.post.err >&2; echo "FAIL: post c0" >&2; exit 1; }

# ---------------------------------------------------------------------
# 2. Fork ?feat and DELETE sub/b.txt there → feat's tree has no b.txt.
# ---------------------------------------------------------------------
"$BE" put '?./feat'      > /dev/null 2>&1 || { echo "FAIL: put ?./feat" >&2; exit 1; }
"$BE" get '?feat'        > /dev/null 2>&1 || { echo "FAIL: get ?feat"   >&2; exit 1; }
"$BE" delete sub/b.txt   > /dev/null 2>&1 || { echo "FAIL: delete b"    >&2; exit 1; }
sleep 0.02
"$BE" post '#feat-del-b' > /dev/null 2> 2.post.err || { cat 2.post.err >&2; echo "FAIL: post feat" >&2; exit 1; }

# Back to trunk: sub/b.txt is present again (trunk still tracks it).
"$BE" get '?..' > /dev/null 2>&1 || { echo "FAIL: get ?.." >&2; exit 1; }
[ -f sub/b.txt ] || { echo "FAIL(setup): trunk should still have sub/b.txt" >&2; exit 1; }

# ---------------------------------------------------------------------
# 3. DRIFT.  Advance the ref to ?feat with sub/ read-only so the unlink
#    of sub/b.txt is lost (best-effort drain swallows the EACCES).  The
#    `get` row + REFS advance to feat (no b.txt) but sub/b.txt survives
#    on disk — the wt-only tracked orphan.  (This is the documented
#    mid-checkout-failure shape; see get/10-partial-recovery.)
# ---------------------------------------------------------------------
chmod 555 sub
set +e
"$BE" get '?feat' > 3.get.out 2> 3.get.err
set -e
chmod 755 sub
[ -f sub/b.txt ] || {
    echo "FAIL(setup): drift did not preserve the orphan sub/b.txt" >&2
    cat 3.get.err >&2
    exit 1
}

# ---------------------------------------------------------------------
# 4. Add a genuinely-UNTRACKED clutter file.  --force (no --prune) must
#    PRESERVE it (GET.mkd: untracked files survive unless pruned).
# ---------------------------------------------------------------------
sleep 0.05; echo junk > sub/clutter.txt

# ---------------------------------------------------------------------
# 5. THE FIX: `be get --force '?feat'`.  baseline == target == feat, so
#    the delta is empty.  Force must STILL:
#      * unlink the wt-only tracked orphan sub/b.txt,
#      * keep a.txt at its target bytes,
#      * preserve the untracked sub/clutter.txt,
#      * leave `be status` clean (no phantom mod/mis).
# ---------------------------------------------------------------------
"$BE" get --force '?feat' > 5.get.out 2> 5.get.err || {
    echo "FAIL: be get --force '?feat'" >&2; cat 5.get.err >&2; exit 1; }

if [ -f sub/b.txt ]; then
    echo "FAIL: --force left the wt-only tracked orphan sub/b.txt on disk" >&2
    echo "      (empty baseline-target delta never reset it — the GET-016 bug)" >&2
    exit 1
fi
[ -f sub/clutter.txt ] || {
    echo "FAIL: --force over-deleted the UNTRACKED sub/clutter.txt" >&2
    echo "      (only --prune may remove untracked files)" >&2
    exit 1
}
[ "$(cat a.txt)" = "A" ] || {
    echo "FAIL: a.txt drifted from target bytes (got '$(cat a.txt)')" >&2
    exit 1
}

# ---------------------------------------------------------------------
# 6. The wt now matches feat exactly; `be status` is clean of tracked
#    drift (the only non-`ok` entry is the untracked clutter, which is
#    correct).  Assert no phantom `mod`/`mis` on tracked paths.
# ---------------------------------------------------------------------
"$BE" status > 6.status.out 2> 6.status.err || { cat 6.status.err >&2; echo "FAIL: status" >&2; exit 1; }
if grep -qE '[[:space:]](mod|mis)[[:space:]]' 6.status.out; then
    echo "FAIL: phantom mod/mis after --force full reset:" >&2
    cat 6.status.out >&2
    exit 1
fi

echo "OK: be get --force reset the drifted wt (tracked orphan unlinked, untracked kept, status clean)"
