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
#      3. Recursion is asserted via the --tlv relay (GET-026): at C2 the
#         stream carries no `vendor/sub` row; on the C2->C3 switch the
#         parent's own rows precede the relayed `vendor/sub` hunk
#         (pre-order).  The old `be: get` stderr markers are now
#         trace-only (ABC_TRACE), so default output carries no debug.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)

# --- Stage 1: clone parent at C2 (no submodule) ----------------------
#  Keeper's wire side fetches by ref name, not raw sha — use the
#  `prev` branch the fixture pushes at PARENT_C2.  Run in --tlv so the
#  relay (not stderr markers) proves there is no sub recursion yet.
"$BE" get "${PARENT_URL}?prev" --tlv >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "stage-1 be get exited $rc; stderr:
$(cat 01.get.got.err)"

[ -f main.c ]              || fail "stage-1: main.c missing"
[ -f util.c ]              || fail "stage-1: util.c missing"
[ ! -e .gitmodules ]       || fail "stage-1: .gitmodules should not exist at C2"
[ ! -e vendor/sub ]        || fail "stage-1: vendor/sub should not exist at C2"
[ ! -e vendor ]            || fail "stage-1: vendor/ should not exist at C2"

#  No subs at C2 → no relayed sub hunk in the --tlv stream.
LC_ALL=C tr '\0' '\n' < 01.get.got.out > 01.tlv.lines
LC_ALL=C grep -aq 'vendor/sub' 01.tlv.lines \
    && fail "stage-1: unexpected 'vendor/sub' relay row at C2:
$(cat -v 01.tlv.lines | head -c 400)"

# --- Stage 2: switch to C3 (introduces the submodule) ----------------
#  master advertises C3 on the bare; fetch+switch via the URL.  --tlv
#  so the relay carries the parent rows + the rebased sub hunk.
"$BE" get "${PARENT_URL}?master" --tlv >02.get.got.out 2>02.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "stage-2 be get exited $rc; stderr:
$(cat 02.get.got.err)"

[ -f .gitmodules ]         || fail "stage-2: .gitmodules missing after C3 switch"
[ -d vendor/sub ]          || fail "stage-2: vendor/sub dir missing"
[ -f vendor/sub/.be ]      || fail "stage-2: vendor/sub/.be anchor missing"
[ -f vendor/sub/core.c ]   || fail "stage-2: vendor/sub/core.c missing"

#  Pre-order recursion via the --tlv relay (GET-026): the parent emits
#  its own report (the `?master#<hashlet>` H-hunk + parent file rows
#  like `main.c`), THEN relays the sub with each row rebased under the
#  mount (`vendor/sub?<hashlet>`).  Pre-order ⇒ the parent's own row
#  sits BEFORE the first `vendor/sub` row in the byte stream.  The TLV
#  stream is NUL-laden binary; split records on NUL into lines so plain
#  `grep -n` (portable, no `-b`) yields stream order.
LC_ALL=C tr '\0' '\n' < 02.get.got.out > 02.tlv.lines
LC_ALL=C grep -aq 'main\.c'    02.tlv.lines \
    || fail "stage-2: no parent 'main.c' row in --tlv relay:
$(cat -v 02.tlv.lines | head -c 400)"
LC_ALL=C grep -aq 'vendor/sub' 02.tlv.lines \
    || fail "stage-2: no relayed 'vendor/sub' sub hunk in --tlv stream:
$(cat -v 02.tlv.lines | head -c 400)"
parent_ln=$(LC_ALL=C grep -anE 'main\.c'    02.tlv.lines | head -1 | cut -d: -f1)
sub_ln=$(   LC_ALL=C grep -anE 'vendor/sub' 02.tlv.lines | head -1 | cut -d: -f1)
[ -n "$parent_ln" ] && [ -n "$sub_ln" ] && [ "$parent_ln" -lt "$sub_ln" ] \
    || fail "stage-2: expected parent row (ln=$parent_ln) before sub hunk (ln=$sub_ln) in --tlv relay"

# --- Sub pin matches what C3 pinned (= SUB_C2) -----------------------
sub_tip=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                      END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
          vendor/sub/.be)
[ "$sub_tip" = "$PARENT_PINNED" ] \
    || fail "sub tip mismatch: got '$sub_tip' want '$PARENT_PINNED'"

note "get/12-sub-add: sub introduced on C2->C3 switch, mounted at SUB_C2"
