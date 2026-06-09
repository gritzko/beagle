#!/bin/sh
#  30-cur-behind-target — DIS-025: `be patch ?<target>` when cur is
#  BEHIND the target must absorb EVERY commit in (cur..target], not
#  just the target's last commit.
#
#  Topology (linear trunk):
#      T0 ── C1(edits a.c) ── C2(edits b.c)      ← trunk
#        \
#         old                                    ← cur, forked at T0 (behind)
#
#  From cur=old (at T0), `be patch ?<C2>` must land BOTH a.c (from C1)
#  and b.c (from C2).  Bug: the merge fork is computed as the target's
#  parent (C1), so a.c's C1 edit is classified `mod` (fork-blob ==
#  theirs-blob) and silently dropped — exit 0.

. "$(dirname "$0")/../../lib/branches.sh"

cp "$CASE/01.a.v0.c" a.c
cp "$CASE/03.b.v0.c" b.c
"$BE" put a.c b.c >/dev/null
"$BE" post 't0' >/dev/null
T0=$(head_hex)

# fork the lagging branch at T0 (cur stays on trunk)
"$BE" put '?./old' >/dev/null

# advance trunk: C1 edits a.c, then C2 edits b.c
sleep 0.02; cp "$CASE/02.a.v1.c" a.c
"$BE" put a.c >/dev/null; "$BE" post 'c1 edit a' >/dev/null
C1=$(head_hex)
sleep 0.02; cp "$CASE/04.b.v1.c" b.c
"$BE" put b.c >/dev/null; "$BE" post 'c2 edit b' >/dev/null
C2=$(head_hex)

# switch to the lagging branch — cur is now BEHIND trunk by C1,C2
"$BE" get '?old' >/dev/null
[ "$(head_hex)" = "$T0" ] || fail "old should be at T0, got $(head_hex)"
match "$CASE/01.a.v0.c" a.c
match "$CASE/03.b.v0.c" b.c

# squash the trunk tip into cur via a short sha-prefix (the real-world
# form).  Whole-branch scope (`!`) must absorb BOTH C1 and C2, not just
# the target's last commit.
HASHLET=$(printf '%s' "$C2" | cut -c1-10)
"$BE" patch "?$HASHLET!" >"$ETMP/p.out" 2>"$ETMP/p.err" \
    || fail "patch ?$HASHLET! failed: $(cat "$ETMP/p.err")"

#  DIS-025 regression check: a.c (C1's edit) must NOT be silently
#  dropped.  Without the fix it stays at v0 and is reported `mod`.
match "$CASE/02.a.v1.c" a.c
match "$CASE/04.b.v1.c" b.c

note "cur-behind-target OK: both C1 (a.c) and C2 (b.c) absorbed"
echo "=== patch/30-cur-behind-target: OK ==="
