#!/bin/sh
#  get/08-weave-marker-line-anchor — `be get` weave-merge must reframe
#  large conflicts at line granularity so picking either side yields
#  a syntactically valid file.
#
#  Pre-fix WEAVEEmitMerged dropped `<<<<` / `||||` / `>>>>` at token
#  boundaries with clusters carrying only the diverging fragment
#  (`LOCAL` / `UPSTREAM`).  Either-side extraction left half-lines:
#      `    if (cond && value > \nLOCAL\n) {` — won't build.
#
#  Fix splits behaviour by size: cluster_bytes * 4 < line_bytes →
#  inline framing (kept compact for single-word swaps); otherwise
#  the reframer splices the shared prefix and suffix into each
#  cluster and pushes markers onto their own lines.  This case
#  exercises the line-level branch.

. "$(dirname "$0")/../../lib/case.sh"

export GIT_CONFIG_GLOBAL=/dev/null
command -v git >/dev/null 2>&1 || {
    echo "run.sh: \`git\` not on PATH" >&2; exit 77
}

BARE="$SCRATCH/bare.git"
SEED="$SCRATCH/seed"

#  1. seed bare repo on master ----------------------------------------
git init --bare "$BARE" >/dev/null
git init "$SEED" >/dev/null
git -C "$SEED" config user.email t@t
git -C "$SEED" config user.name  T
git -C "$SEED" checkout -b master >/dev/null 2>&1 || true
cp "$CASE/01.initial.txt" "$SEED/f.txt"
git -C "$SEED" add f.txt >/dev/null
git -C "$SEED" commit -qm 'v1'
git -C "$SEED" push -q "$BARE" master:master

#  2. clone via `be get file://` --------------------------------------
rm -rf "$SCRATCH/.be"
mkdir wt && cd wt
"$BE" get "file://$BARE?master" \
    > 01.get.got.out 2> 01.get.got.err || {
    echo "FAIL: initial be get failed" >&2
    cat 01.get.got.err >&2
    exit 1
}
[ -f f.txt ] || { echo "FAIL: wt/f.txt missing after initial clone" >&2; exit 1; }

#  3. apply wt edit + upstream edit -----------------------------------
cp "$CASE/02.wt-edit.txt"       f.txt
cp "$CASE/03.upstream-edit.txt" "$SEED/f.txt"
git -C "$SEED" commit -aqm 'v2'
git -C "$SEED" push -q "$BARE" master:master

#  4. be get again — should weave-merge with conflict markers ---------
"$BE" get "file://$BARE?master" \
    > 02.get.got.out 2> 02.get.got.err || {
    echo "FAIL: second be get failed" >&2
    cat 02.get.got.err >&2
    exit 1
}

#  5. markers must be line-anchored (column 0, followed by \n) --------
awk 'BEGIN { RS=""; ORS="" }
     {
        n = length($0)
        bad = 0
        for (mk_i = 1; mk_i <= 3; mk_i++) {
            mk = (mk_i == 1 ? "<<<<" : (mk_i == 2 ? "||||" : ">>>>"))
            pos = 1
            while ((idx = index(substr($0, pos), mk)) > 0) {
                abs = pos + idx - 1
                prev = (abs == 1) ? "\n" : substr($0, abs - 1, 1)
                next_c = (abs + 4 > n) ? "\n" : substr($0, abs + 4, 1)
                if (prev != "\n" || next_c != "\n") {
                    printf("displaced %s at byte %d: prev=%s next=%s\n",
                           mk, abs, (prev == "\n" ? "LF" : prev),
                           (next_c == "\n" ? "LF" : next_c)) > "/dev/stderr"
                    bad = 1
                }
                pos = abs + 4
            }
        }
        if (bad) exit 1
     }' f.txt || {
    echo "FAIL: f.txt has displaced conflict markers:" >&2
    cat -A f.txt >&2
    exit 1
}

#  6. markers fired AND each cluster carries the full conflict line.
#     The wt edit and upstream edit each rewrote the same line; the
#     reframer must splice the shared prefix `    if (cond && value > `
#     and suffix `) {` into both clusters.  Pre-fix the cluster
#     contained only `LOCAL` / `UPSTREAM`.
grep -q '^<<<<' f.txt && grep -q '^||||' f.txt && grep -q '^>>>>' f.txt || {
    echo "FAIL: expected line-anchored markers in f.txt — none found" >&2
    cat -A f.txt >&2; exit 1
}
grep -q '^    if (cond && value > LOCAL) {$' f.txt || {
    echo "FAIL: LOCAL side is not a complete line in f.txt" >&2
    cat -A f.txt >&2; exit 1
}
grep -q '^    if (cond && value > UPSTREAM) {$' f.txt || {
    echo "FAIL: UPSTREAM side is not a complete line in f.txt" >&2
    cat -A f.txt >&2; exit 1
}
