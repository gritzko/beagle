#!/bin/sh
#  head/03-sub-clean-flat — `be head ?` recurses into a clean
#  submodule and reports both projects.  Canary for the per-verb
#  recursion mechanism described in SUBS.plan.md.
#
#  Setup (from submodules.sh):
#      parent.git/  — bare git upstream, 3 commits, .gitmodules pins
#                     vendor/sub to sub's C2.
#      sub.git/     — bare git upstream, 3 commits.
#
#  Test:
#      1. Clone parent into wt/ — sub mounts at vendor/sub.
#      2. `be head ?` — should visit both projects (outer + vendor/sub)
#         in pre-order.  Neither has committed work past its current
#         tip, so each project reports clean (ahead/behind 0/0).
#      3. Read-only invariant: parent's `.be/wtlog` and the sub's
#         `vendor/sub/.be` ULOG line count must be unchanged after.
#
#  Recursion-marker contract: `be` writes one stderr line per project
#  visited, of the form `be: head <relpath>`.  `<relpath>` is `.` for
#  the outermost project and the sub's mount path for each child.
#
#  Status: WILL_FAIL until beagle/SUBS recursion lands (see
#  SUBS.plan.md "Sequencing" steps 2-3).

. "$(dirname "$0")/../../lib/submodules.sh"

# --- 1. Clone parent — auto-mounts vendor/sub via .gitmodules. -------
mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err

[ -f main.c ]               || fail "parent main.c missing"
[ -d vendor/sub ]           || fail "sub mount dir missing"
[ -f vendor/sub/.be ]       || fail "sub anchor file missing"
[ ! -d vendor/sub/.be ]     || fail "sub .be must be a regular file"
[ -f vendor/sub/core.c ]    || fail "sub content missing"

# --- 2. Snapshot wtlog line counts (read-only invariant). ------------
parent_wtlog=$(wtlog_path)
parent_lines_before=$(wc -l < "$parent_wtlog")
sub_lines_before=$(wc -l < vendor/sub/.be)

# --- 3. `be head ?` — recurse across parent + sub. -------------------
"$BE" head '?' >02.head.got.out 2>02.head.got.err
rc=$?
[ "$rc" = 0 ] || fail "be head exited $rc; stderr:
$(cat 02.head.got.err)"

# Outer project marker on stderr.
grep -Eq '^be: head [.]( |$)|^be: head [.]$' 02.head.got.err \
    || fail "missing outer-project head marker on stderr:
$(cat 02.head.got.err)"

# Sub project marker on stderr — proves recursion fired.
grep -q '^be: head vendor/sub' 02.head.got.err \
    || fail "be head did not recurse into vendor/sub; stderr:
$(cat 02.head.got.err)"

# Pre-order: outer marker appears before the sub marker.
outer_line=$(grep -n '^be: head [.]' 02.head.got.err | head -1 | cut -d: -f1)
sub_line=$(grep -n '^be: head vendor/sub' 02.head.got.err | head -1 | cut -d: -f1)
[ -n "$outer_line" ] && [ -n "$sub_line" ] && [ "$outer_line" -lt "$sub_line" ] \
    || fail "expected outer marker (line=$outer_line) before sub marker (line=$sub_line); stderr:
$(cat 02.head.got.err)"

# --- 4. Read-only invariant. -----------------------------------------
parent_lines_after=$(wc -l < "$parent_wtlog")
sub_lines_after=$(wc -l < vendor/sub/.be)
[ "$parent_lines_after" = "$parent_lines_before" ] \
    || fail "parent .be/wtlog grew: $parent_lines_before -> $parent_lines_after"
[ "$sub_lines_after" = "$sub_lines_before" ] \
    || fail "sub .be (wtlog file) grew: $sub_lines_before -> $sub_lines_after"

note "head/03-sub-clean-flat: outer + vendor/sub visited in pre-order"
