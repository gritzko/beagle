#!/bin/sh
#  head/01-ahead-behind — `be head ?..` on a diverged feature
#  branch surfaces the ahead/behind commit lists.  Multi-branch
#  scenario:
#
#    trunk   T1 ── T2 ── T3
#               \
#    ?./feat   F1 ── F2          (cur during the test)
#
#  Expected output: log-format lines (`<short-sha>  HH:MM  msg
#  (author)`) prefixed `+` (ahead-of-target) or `-` (behind), per
#  the user-supplied HEAD format.  Newest-first within each side.
#
#  Status: HEAD's diff-summary half is unimplemented today
#  (BEHead in beagle/BE.cli.c only routes the fetch leg; the
#  cur-vs-ref ahead/behind entry point in sniff/graf doesn't
#  exist yet).  Test is registered with WILL_FAIL=TRUE in
#  test/CMakeLists.txt; flip when impl lands.

. "$(dirname "$0")/../../lib/case.sh"

cd "$SCRATCH"

# Anchor project shard at .be/$P/ so subsequent be invocations
# don't derive the project name from the first URI's basename.
"$BE" put "?/$P/" 2>/dev/null || true

# --- T1 on trunk ---
echo "t1" > shared.txt
"$BE" put shared.txt >/dev/null
"$BE" post '#T1' >/dev/null

# --- fork feature at T1 ---
"$BE" put '?./feat' >/dev/null
"$BE" get '?./feat' >/dev/null

# --- F1 on feature ---
echo "f1" > c.txt
"$BE" put c.txt >/dev/null
"$BE" post '#F1' >/dev/null

# --- F2 on feature ---
echo "f2" > d.txt
"$BE" put d.txt >/dev/null
"$BE" post '#F2' >/dev/null

# --- back to trunk; T2 advances trunk ---
"$BE" get '?..' >/dev/null
echo "t2" > a.txt
"$BE" put a.txt >/dev/null
"$BE" post '#T2' >/dev/null

# --- T3 on trunk ---
echo "t3" > b.txt
"$BE" put b.txt >/dev/null
"$BE" post '#T3' >/dev/null

# --- switch to feature; cur is now diverged from trunk ---
"$BE" get '?./feat' >/dev/null

# --- be head ?.. — implicit-target form (parent = trunk) ---
"$BE" head '?..' >01.head_parent.got.out 2>01.head_parent.got.err
match_re "$CASE/01.head_parent.want.txt" 01.head_parent.got.out

# --- be head ?  — explicit absolute trunk ---
"$BE" head '?'   >02.head_trunk.got.out 2>02.head_trunk.got.err
match_re "$CASE/02.head_trunk.want.txt" 02.head_trunk.got.out

# --- HEAD must NOT mutate cur (no commit, no ref move) ---
TIP_BEFORE=$(awk -F'\t' '$2=="post"{last=$3} END{
    h=last; sub(/^[^#]*#/, "", h); print h }' .be/wtlog)
"$BE" head '?..' >/dev/null 2>&1
TIP_AFTER=$(awk -F'\t' '$2=="post"{last=$3} END{
    h=last; sub(/^[^#]*#/, "", h); print h }' .be/wtlog)
[ "$TIP_AFTER" = "$TIP_BEFORE" ] || {
    echo "head: cur tip changed after read-only HEAD" >&2
    exit 1
}
