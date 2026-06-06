#!/bin/sh
#  put/09-sub-only-dirty-exit — SUBS-004 repro.  Outer parent is left
#  clean; only a mounted submodule's file is edited.  Bare `be put`
#  recurses, stages the sub blob (the sub's wtlog gains a real `put
#  core.c` row, and stdout lists `put vendor/sub/core.c`) — so the bare
#  put did real work and MUST exit 0.
#
#  Before the fix it exited 206/PUTNONE: the parent sniff's PUTNONE
#  ("no changes") propagated as the command exit, masking the
#  successful sub stage, because BEActSubsRelay ran AFTER the parent
#  put and its return never cleared the executor's stale last-NONE.
#
#  Symmetric control: put/01 (parent-only) and the both-dirty path
#  both already exit 0; the truly-empty case (parent clean AND sub
#  clean) must still surface PUTNONE — asserted at the tail.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"

[ -f vendor/sub/core.c ] || fail "vendor/sub/core.c missing after get"

#  Baseline sub wtlog length, so we can prove a put row was appended.
sub_rows_before=$(wc -l < vendor/sub/.be)

#  Edit ONLY the sub.  The outer worktree is left untouched.
sleep 0.02
cat >> vendor/sub/core.c <<'EOF'

void sub_zap(void) { sub_counter = 0; }
EOF

#  THE bug: bare `be put` with only the sub dirty must exit 0.
"$BE" put >02.put.got.out 2>02.put.got.err
rc=$?
[ "$rc" = 0 ] || fail "bare be put (sub-only dirty) exited $rc, want 0; stderr:
$(cat 02.put.got.err)"

#  The sub blob really got staged — stdout lists the path-prefixed row.
grep -q "put	vendor/sub/core.c" 02.put.got.out \
    || grep -q "put.*vendor/sub/core.c" 02.put.got.out \
    || fail "put stdout did not list vendor/sub/core.c; dump:
$(cat 02.put.got.out)"

#  …and the sub's own wtlog gained a real `put core.c` row.
sub_rows_after=$(wc -l < vendor/sub/.be)
[ "$sub_rows_after" -gt "$sub_rows_before" ] \
    || fail "sub wtlog did not grow ($sub_rows_before -> $sub_rows_after)"
tail -1 vendor/sub/.be | grep -q "put	core.c" \
    || fail "sub wtlog tail is not a 'put core.c' row:
$(tail -1 vendor/sub/.be)"

#  Must-still-PUTNONE: a truly-empty bare put (parent clean AND every
#  sub clean) still surfaces PUTNONE.  Use a fresh, untouched clone —
#  a re-`put` of the SAME wt is not "clean" (the sub's edit is staged
#  but its on-disk content still diverges from its baseline until a
#  `be post`), so it would re-stage; only a never-edited clone is clean.
cd ..
mkdir wt2 wt2/.be && cd wt2
"$BE" get "$PARENT_URL?master" >04.get.got.out 2>04.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "second be get exited $rc; stderr:
$(cat 04.get.got.err)"

#  Expected to fail (PUTNONE) — guard so `set -e` doesn't abort here.
rc=0
"$BE" put >05.put.got.out 2>05.put.got.err || rc=$?
[ "$rc" != 0 ] \
    || fail "bare be put on a clean clone (parent + all subs clean) \
unexpectedly exited 0; PUTNONE must surface for a truly-empty put; stdout:
$(cat 05.put.got.out)"

note "put/09: sub-only-dirty bare put exits 0; empty clone still PUTNONE"
