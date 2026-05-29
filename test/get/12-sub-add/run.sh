#!/bin/sh
#  get/12-sub-add — switching from a parent commit that has no sub
#  (PARENT_C2) to one that introduces the sub (PARENT_C3) mounts the
#  sub on the second `be get` without needing a fresh clone.
#
#  Regression: stage 2 used to fail with "keeper: subs: commit not
#  found" (KEEPNONE).  Root cause was in the wire-fetch shard
#  placement, not sniff: `be get ssh://…?master` while cur=`prev`
#  landed C3's pack in the `prev/` leaf shard (the stale active
#  leaf), so a later `keeper subs ?master#<C3>` opening trunk could
#  not reach C3 (object resolution only walks child→parent→root,
#  never into a sibling leaf).  Fixed in keeper/KEEP.cli.c: wire
#  fetches now always land objects in the project trunk (root),
#  which is reachable from every branch.
#
#  Setup (from submodules.sh):
#      parent.git/  — 3 commits.  C1 main.c only, C2 + util.c (no sub),
#                     C3 introduces .gitmodules + 160000 vendor/sub.
#
#  Test:
#      1. `be get ssh://...parent.git?<PARENT_C2-hex>` — clone parent
#         at C2.  No .gitmodules, no vendor/sub mount.
#      2. `be get ?<PARENT_C3-hex>` — switch to C3.  Sub should now
#         materialise.
#      3. Both stages emit recursion markers as appropriate.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)

# --- Stage 1: clone parent at C2 (no submodule) ----------------------
#  Keeper's wire side fetches by ref name, not raw sha — use the
#  `prev` branch the fixture pushes at PARENT_C2.
"$BE" get "${PARENT_URL}?prev" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "stage-1 be get exited $rc; stderr:
$(cat 01.get.got.err)"

[ -f main.c ]              || fail "stage-1: main.c missing"
[ -f util.c ]              || fail "stage-1: util.c missing"
[ ! -e .gitmodules ]       || fail "stage-1: .gitmodules should not exist at C2"
[ ! -e vendor/sub ]        || fail "stage-1: vendor/sub should not exist at C2"
[ ! -e vendor ]            || fail "stage-1: vendor/ should not exist at C2"

#  No subs at C2 → no recursion markers expected.
grep -q '^be: get '         01.get.got.err \
    && fail "stage-1: unexpected recursion marker on stderr:
$(cat 01.get.got.err)"

# --- Stage 2: switch to C3 (introduces the submodule) ----------------
#  master advertises C3 on the bare; fetch+switch via the URL.
"$BE" get "${PARENT_URL}?master" >02.get.got.out 2>02.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "stage-2 be get exited $rc; stderr:
$(cat 02.get.got.err)"

[ -f .gitmodules ]         || fail "stage-2: .gitmodules missing after C3 switch"
[ -d vendor/sub ]          || fail "stage-2: vendor/sub dir missing"
[ -f vendor/sub/.be ]      || fail "stage-2: vendor/sub/.be anchor missing"
[ -f vendor/sub/core.c ]   || fail "stage-2: vendor/sub/core.c missing"

#  Recursion markers — both outer + sub on the switch.
grep -q '^be: get [.]'        02.get.got.err \
    || fail "stage-2: outer marker missing; stderr:
$(cat 02.get.got.err)"
grep -q '^be: get vendor/sub' 02.get.got.err \
    || fail "stage-2: sub marker missing; stderr:
$(cat 02.get.got.err)"

# --- Sub pin matches what C3 pinned (= SUB_C2) -----------------------
sub_tip=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                      END { h=last; sub(/^[^#]*#/, "", h); print h }' \
          vendor/sub/.be)
[ "$sub_tip" = "$PARENT_PINNED" ] \
    || fail "sub tip mismatch: got '$sub_tip' want '$PARENT_PINNED'"

note "get/12-sub-add: sub introduced on C2->C3 switch, mounted at SUB_C2"
