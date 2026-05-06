#!/bin/sh
#  03-multi-edit-merge — exercise PATCH against a richly-evolved C
#  file: 5 commits on parent (2 pre-fork, 3 post-fork) and 3 commits
#  on a child branch, each commit bundling several edits (token
#  renames, deletes, body refactors, format changes, additions).
#  After `be patch ?./child` from cur=parent, every change from both
#  branches must land in the wt; `be post 'all merged'` then commits
#  the merged tree as a single-parent commit on parent.
#
#  Edit map (all per-commit edits land in disjoint sections so the
#  3-way merge cleanly takes ours / theirs / base for each):
#
#    T0  baseline                           (math+string+io w/ placeholders)
#    T1  parent c1: rename x,y → a,b in add+sub; delete placeholder_m
#    T2  parent c2: K&R reformat ALL fns; refactor add body; add mul
#         (fork point — child branch off here)
#    C1  child c1: rename greet_str → hello, bye_str → goodbye;
#                  delete placeholder_s
#    C2  child c2: change "hi" → "Hello!"; add welcome const
#    C3  child c3: change "bye" → "Farewell."; add thanks const
#    T3  parent c3 (post-fork): rename log_msg → print_line; delete
#                               placeholder_io
#    T4  parent c4: change "%s\n" → "[log] %s\n" in print_line;
#                   add print_err
#
#  3-way result:
#    math   — base==ours==theirs (no change)
#    string — ours==base, theirs differs → take theirs (child edits)
#    io     — ours differs, theirs==base → take ours (parent edits)

. "$(dirname "$0")/../../lib/case.sh"

OUT="$SCRATCH/../out"
mkdir -p "$OUT"

# T0 baseline
sleep 0.02; cp "$CASE/01.lib.t0.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 'baseline init' >/dev/null

# T1 parent c1
sleep 0.02; cp "$CASE/02.lib.t1.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't1 rename and prune' >/dev/null

# T2 parent c2 (K&R reformat + refactor + add mul) — fork point
sleep 0.02; cp "$CASE/03.lib.t2.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't2 KR reformat add mul' >/dev/null

# Fork the child branch off T2
"$BE" put '?./child' >/dev/null
"$BE" get '?child'   >/dev/null

# C1 child c1
sleep 0.02; cp "$CASE/04.lib.c1.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 'c1 string renames and prune' >/dev/null

# C2 child c2
sleep 0.02; cp "$CASE/05.lib.c2.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 'c2 hello and welcome' >/dev/null

# C3 child c3
sleep 0.02; cp "$CASE/06.lib.c3.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 'c3 farewell and thanks' >/dev/null

# Switch back to parent (trunk)
"$BE" get '?..' >/dev/null

# T3 parent c3 (post-fork)
sleep 0.02; cp "$CASE/07.lib.t3.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't3 rename log_msg and prune' >/dev/null

# T4 parent c4
sleep 0.02; cp "$CASE/08.lib.t4.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't4 format and print_err' >/dev/null

# Merge child into parent's wt via 3-way patch
"$BE" patch '?./child' >"$OUT/patch.out" 2>"$OUT/patch.err"

# Verify wt holds the merged result with all edits from both sides
match "$CASE/09.lib.want.c" lib.c

# Commit the merged tree as a single-parent commit on parent
"$BE" post 'all merged' >"$OUT/post.out" 2>"$OUT/post.err"
