#!/bin/sh
#  patch/34-sub-gitlink-bump-staged — REPRO for PATCH-001.  `be patch` of
#  a behind source whose ONLY forward change is a submodule gitlink bump
#  must STAGE the parent gitlink row (take-theirs), so a plain `be post`
#  records the new pin — NO manual `be put <sub>` needed.
#
#  Bug: sniff PATCH classified EVERY gitlink leaf as `noop` and skipped
#  it.  The submodule-recursion arm (BEActSubsPatch) still re-got the sub
#  on disk at the new pin, but the parent tree kept pinning the OLD commit
#  → a half-applied state: `be status` clean for the gitlink, and the next
#  `be post` recorded nothing (POSTNONE in selective mode).  Surfaced
#  landing MEM-010.  Fix: a forward-moved gitlink (theirs pin advanced past
#  ours) is take-theirs and stages a `put <sub>#<new-pin>` row.
#
#  Topology — cur (master) is AHEAD in regular files, BEHIND in the pin:
#      C3(pin C2) ── master'(advance util.c)        ← cur=master
#                 \
#                  bump(pin C2 -> C3)               ← source, behind master
#
#  Patching master with `?bump` absorbs ONLY the gitlink bump.  Assert:
#    1. patch counts take-theirs >= 1 (not all-noop).
#    2. a `put vendor/sub#<C3-pin>` row is staged in the wtlog.
#    3. the sub is re-got to C3 on disk (recursion arm, unchanged).
#    4. a plain `be post --nosub` (no manual put) records the bump.
#    5. the committed parent tree pins C3.

. "$(dirname "$0")/../../lib/submodules.sh"

#  --- Build the two branches off a throwaway clone of the bare parent.
#  `bump` forks master and moves the gitlink C2 -> C3 only; then master
#  advances with an unrelated regular-file commit so cur is AHEAD of the
#  source in everything BUT the pin (the exact PATCH-001 shape).
ADV="$ETMP/adv-work"
rm -rf "$ADV"
git clone -q "$PARENT_BARE" "$ADV"
git -C "$ADV" config user.email t@t
git -C "$ADV" config user.name  T

git -C "$ADV" checkout -q -b bump master
git -C "$ADV" update-index --cacheinfo 160000,"$SUB_C3",vendor/sub
git -C "$ADV" commit -qm 'bump: vendor/sub C2 -> C3'
git -C "$ADV" push -q "$PARENT_BARE" bump:refs/heads/bump

git -C "$ADV" checkout -q master
printf '\nint extra(void){return 7;}\n' >> "$ADV/util.c"
git -C "$ADV" commit -qam 'master: advance util.c'
git -C "$ADV" push -q "$PARENT_BARE" master:refs/heads/master
note "patch/34: bump moves vendor/sub $SUB_C2 -> $SUB_C3; master advanced util.c"

#  --- Clone the parent at master; sub mounts at C2 (no sub_add).
mkdir wt wt/.be && cd wt
"$BE" get "$PARENT_URL?master" >"$ETMP/01.get.out" 2>"$ETMP/01.get.err" \
    || fail "be get exited $?; stderr:
$(cat "$ETMP/01.get.err")"
[ -f vendor/sub/core.c ] || fail "vendor/sub/core.c missing (sub not mounted)"
grep -q 'sub_add' vendor/sub/core.c \
    && fail "precondition: vendor/sub already at C3 (should be C2)"

#  --- Patch the behind source whose only forward change is the pin.
"$BE" patch "$PARENT_URL?bump" >"$ETMP/02.patch.out" 2>"$ETMP/02.patch.err"
rc=$?
[ "$rc" = 0 ] || fail "be patch exited $rc (expected 0); stderr:
$(cat "$ETMP/02.patch.err")"

#  (1) the gitlink was absorbed as take-theirs, NOT silently dropped to noop.
grep -Eq 'take-theirs=[1-9]' "$ETMP/02.patch.out" \
    || fail "PATCH-001: gitlink bump not absorbed (take-theirs=0); patch said:
$(grep 'sniff: patch:' "$ETMP/02.patch.err")"

#  (2) the parent gitlink put-row is staged (the load-bearing fix).
grep -Eq "^[^	]*	put	vendor/sub#$SUB_C3\$" .be/wtlog \
    || fail "PATCH-001: no 'put vendor/sub#$SUB_C3' row staged; wtlog:
$(cat .be/wtlog)"

#  (3) the sub advanced to the new pin on disk (recursion arm — unchanged).
grep -q 'sub_add' vendor/sub/core.c \
    || fail "PATCH-001: sub was NOT re-got to the patched pin; core.c:
$(cat vendor/sub/core.c)"

#  (4) a plain `be post` (selective mode, NO manual `be put`) records the
#  bump.  --nosub keeps this assertion about the PARENT commit only.
"$BE" post --nosub '#land vendor/sub bump' \
    >"$ETMP/03.post.out" 2>"$ETMP/03.post.err"
prc=$?
[ "$prc" = 0 ] || fail "be post exited $prc (expected 0); stderr:
$(cat "$ETMP/03.post.err")"

#  (5) the committed parent tree pins C3 (not the stale C2).
"$BE" --nosub tree:vendor?master >"$ETMP/04.tree.out" 2>"$ETMP/04.tree.err"
grep -q "$SUB_C3" "$ETMP/04.tree.out" \
    || fail "PATCH-001: committed parent tree does NOT pin C3 ($SUB_C3); tree:
$(cat "$ETMP/04.tree.out")"
grep -q "$SUB_C2" "$ETMP/04.tree.out" \
    && fail "PATCH-001: committed parent tree still pins stale C2 ($SUB_C2); tree:
$(cat "$ETMP/04.tree.out")"

note "patch/34 ok: behind-source gitlink bump staged + committed (no manual put)"
echo "=== patch/34-sub-gitlink-bump-staged: OK ==="
