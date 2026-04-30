#!/bin/sh
. "$(dirname "$0")/../../lib/case.sh"

# Two child branches, fix1 and fix2, fork off trunk and each grow two
# commits; both are then promoted up into trunk via `be post ?..`.
# After both promotes, trunk's tip carries fix1's a.txt edit and
# fix2's b.txt edit.  The first promote is a fast-forward (trunk
# unchanged); the second triggers a real rebase (trunk has advanced
# past fix1's fork point with fix2's commits).
#
# After the rebase-promote, POST does NOT auto-refresh the wt — it
# updates REFS but leaves the on-disk tree at its pre-rebase state.
# An explicit `be get ?fix1` materialises the rebased tree.  This
# behaviour is documented inline at the assertion site.

OUT="$SCRATCH/../out"
mkdir -p "$OUT"

# Step 1: trunk baseline — a.txt + b.txt
cp "$CASE/01.a.txt" a.txt
cp "$CASE/02.b.txt" b.txt
"$BE" put a.txt b.txt >/dev/null 2>&1
"$BE" post baseline   >/dev/null 2>&1

# Step 2: fork ?fix1 (cur stays on trunk)
"$BE" post '?./fix1' >/dev/null 2>&1

# Step 3: switch to fix1, two commits editing a.txt
"$BE" get '?fix1' >/dev/null 2>&1
cp "$CASE/03.a-fix1-c1.txt" a.txt
"$BE" put a.txt >/dev/null 2>&1
"$BE" post c1   >/dev/null 2>&1
cp "$CASE/04.a-fix1-c2.txt" a.txt
"$BE" put a.txt >/dev/null 2>&1
"$BE" post c2   >/dev/null 2>&1

# Step 4: back to trunk, fork ?fix2
"$BE" get '?..'      >/dev/null 2>&1
"$BE" post '?./fix2' >/dev/null 2>&1

# Step 5: switch to fix2, two commits editing b.txt
"$BE" get '?fix2' >/dev/null 2>&1
cp "$CASE/05.b-fix2-c1.txt" b.txt
"$BE" put b.txt >/dev/null 2>&1
"$BE" post c1   >/dev/null 2>&1
cp "$CASE/06.b-fix2-c2.txt" b.txt
"$BE" put b.txt >/dev/null 2>&1
"$BE" post c2   >/dev/null 2>&1

# Step 6: from fix2, promote into trunk via `be post ?..`.
# Trunk hasn't moved since fix2 forked — fast-forward path.
must "$BE" post '?..' >/dev/null 2>&1

# Step 7: switch to fix1, then promote it into trunk.
# Trunk has now advanced past fix1's fork point (carries fix2's
# commits), so fix1's promote triggers a real rebase.
"$BE" get '?fix1' >/dev/null 2>&1
must "$BE" post '?..' >/dev/null 2>&1

# Step 8: switch to trunk and refresh the wt.
# (POST advances REFS but doesn't refresh the on-disk tree — this
# explicit GET materialises the post-promote state.)
"$BE" get '?..' >/dev/null 2>&1

# Final assertion: trunk's wt has both edits.
match "$CASE/07.a.want.txt" a.txt
match "$CASE/08.b.want.txt" b.txt
