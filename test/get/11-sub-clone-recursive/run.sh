#!/bin/sh
#  get/11-sub-clone-recursive — `be get $PARENT_URL?master` mounts the
#  parent project plus its declared submodule end-to-end.
#
#  Setup (from submodules.sh):
#      parent.git/  — bare git upstream, 3 commits, .gitmodules pins
#                     vendor/sub to sub's C2.
#      sub.git/     — bare git upstream, 3 commits.
#
#  Test:
#      1. `be get ssh://localhost/<rel>/parent.git?master` into wt/.
#      2. Assert parent files materialised (main.c, util.c, .gitmodules).
#      3. Assert sub mount: vendor/sub/.be is a regular-file anchor
#         and vendor/sub/core.c materialised at SUB_C2's content.
#      4. Assert pre-order recursion via the --tlv relay (GET-026): the
#         parent's own H-hunk + file rows precede the relayed sub hunk
#         (`vendor/sub?<hashlet>`).  The old `be: get` stderr markers are
#         now trace-only (ABC_TRACE), so default output carries no debug.
#      5. Assert sub's recorded tip matches $PARENT_PINNED (= $SUB_C2).

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"

# --- parent files materialised ---------------------------------------
[ -f main.c ]        || fail "wt/main.c missing"
[ -f util.c ]        || fail "wt/util.c missing"
[ -f .gitmodules ]   || fail "wt/.gitmodules missing"
grep -q 'vendor/sub' .gitmodules \
    || fail ".gitmodules content unexpected: $(cat .gitmodules)"

# --- sub mounted as secondary wt -------------------------------------
[ -d vendor/sub ]            || fail "vendor/sub dir missing"
[ -f vendor/sub/.be ]        || fail "vendor/sub/.be anchor missing (not a file)"
[ ! -d vendor/sub/.be ]      || fail "vendor/sub/.be should be a regular file"
[ -f vendor/sub/core.c ]     || fail "vendor/sub/core.c missing"

#  Sub is pinned at SUB_C2 per .gitmodules; that commit's core.c lacks
#  sub_add but has sub_inc.
grep -q 'sub_inc' vendor/sub/core.c \
    || fail "sub/core.c missing sub_inc (expected at SUB_C2):
$(cat vendor/sub/core.c)"

# --- pre-order recursion via the --tlv relay (GET-026) ---------------
#  A fresh sibling wt B re-runs the same get in --tlv: the parent emits
#  its own report (the `?master#<hashlet>` H-hunk + parent file rows like
#  `main.c`), THEN relays the sub with each row rebased under the mount
#  (`vendor/sub?<hashlet>`).  Pre-order ⇒ the parent's own rows sit
#  BEFORE the first `vendor/sub` row in the byte stream.  No reliance on
#  debug stderr markers (now ABC_TRACE-gated); the relay IS the proof.
cd "$SCRATCH"
mkdir B B/.be && cd B
"$BE" get "$PARENT_URL?master" --tlv >01.tlv.out 2>01.tlv.err
rc=$?
[ "$rc" = 0 ] || fail "be get --tlv exited $rc; stderr:
$(cat 01.tlv.err)"

#  The TLV stream is NUL-laden binary; split records on NUL into lines
#  so plain `grep -n` (portable, no `-b`) yields stream order.
LC_ALL=C tr '\0' '\n' < 01.tlv.out > 01.tlv.lines
LC_ALL=C grep -aq 'main\.c'    01.tlv.lines \
    || fail "no parent 'main.c' row in --tlv relay:
$(cat -v 01.tlv.lines | head -c 400)"
LC_ALL=C grep -aq 'vendor/sub' 01.tlv.lines \
    || fail "no relayed 'vendor/sub' sub hunk in --tlv stream:
$(cat -v 01.tlv.lines | head -c 400)"

#  The parent's own row must precede the sub's mount hunk in the stream.
parent_ln=$(LC_ALL=C grep -anE 'main\.c'    01.tlv.lines | head -1 | cut -d: -f1)
sub_ln=$(   LC_ALL=C grep -anE 'vendor/sub' 01.tlv.lines | head -1 | cut -d: -f1)
[ -n "$parent_ln" ] && [ -n "$sub_ln" ] && [ "$parent_ln" -lt "$sub_ln" ] \
    || fail "expected parent row (ln=$parent_ln) before sub hunk (ln=$sub_ln) in --tlv relay"
cd "$SCRATCH/wt"

# --- sub's recorded pin matches what .gitmodules pinned (SUB_C2) -----
sub_tip=$(awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
                      END { h=last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h); print h }' \
          vendor/sub/.be)
[ "$sub_tip" = "$PARENT_PINNED" ] \
    || fail "sub tip mismatch: got '$sub_tip' want '$PARENT_PINNED'"

# --- sub-shard carries the sub's OWN keeper data ---------------------
#  Per SUBS.plan.md §"Storage layout": parent_root/.be/<basename>/ is
#  the sub's private keeper shard.  The recursive mount currently
#  short-circuits and lets sub objects piggy-back on parent's trunk,
#  leaving .be/sub/ empty — that's the bug.  This assertion pins the
#  intended invariant: the shard must hold the sub's refs reflog at
#  minimum (a real fetch also drops a keeper pack run in there).
[ -d .be/sub ] || fail ".be/sub shard dir missing"
[ -s .be/sub/refs ] \
    || fail ".be/sub/refs missing or empty — sub keeper data not landing in shard:
$(ls -la .be/sub 2>&1)"

note "get/11-sub-clone-recursive: parent + vendor/sub mounted at SUB_C2"
