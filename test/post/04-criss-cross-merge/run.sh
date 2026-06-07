#!/bin/sh
#  04-criss-cross-merge — exercise WEAVEMerge() against a real
#  two-LCA criss-cross DAG.  dogs's POST is single-parent only
#  (https://replicated.wiki/html/wiki/Invariants.html Invariant 2), so the criss-cross history can't be
#  built with `be`; we seed it via `git commit-tree` plumbing into
#  a bare git repo, then `be get file://…?fix2` imports the
#  multi-parent topology over the keeper-side git-upload-pack
#  shim.  Once imported, `be patch ?fix1` on the wt feeds two
#  weaves with disagreeing pre-LCA `in` stamps into WEAVEMerge.
#
#  Topology built in the bare git repo:
#
#         T1 (a=alpha, b=beta, c=gamma)            refs/heads/main
#        /  \
#       C1   C2                  C1: a → alpha-fix1
#       |     |                  C2: b → beta-fix2
#       M1   M2                  M1 (-p C1 -p C2): + c → gamma-m1
#                                M2 (-p C2 -p C1): + b → beta-fix2-m2
#       (refs/heads/fix1 -> M1 ; refs/heads/fix2 -> M2)
#
#  After `be get file://…?fix2` the wt sits at M2:
#       a = alpha-fix1, b = beta-fix2-m2, c = gamma
#
#  `be patch ?fix1` 3-ways {ours = M2, theirs = M1, base = lca}.
#  Per-file expected outcome:
#    a — both sides agree (alpha-fix1) → no change.
#    b — ours edited (beta-fix2 → beta-fix2-m2), theirs matches an
#        LCA tree (C2's beta-fix2): take ours.  Picking the wrong
#        LCA would produce a spurious conflict — the criss-cross
#        property under test.
#    c — theirs edited (gamma → gamma-m1), ours matches an LCA
#        (C1's gamma): take theirs.
#
#  `be post sync` then commits the merged tree on ?fix2 as a single-
#  parent commit (PATCH absorbed without recording M1 as parent).
#
#  Asserts:
#    * `be get file://…` succeeds against a bare git repo (WIRECLI
#      git-over-file dispatch).
#    * `be patch ?fix1` succeeds with no conflict markers.
#    * Post-merge wt content matches the want files.
#    * The new tip on ?fix2 is single-parent (https://replicated.wiki/html/wiki/Invariants.html Invariant 2)
#      with parent == M2's imported sha.

. "$(dirname "$0")/../../lib/case.sh"

KEEPER=${KEEPER:-${BIN:+$BIN/keeper}}
KEEPER=${KEEPER:-$(command -v keeper || true)}
[ -n "$KEEPER" ] && [ -x "$KEEPER" ] || {
    echo "run.sh: cannot locate \`keeper\`" >&2
    exit 2
}
command -v git >/dev/null 2>&1 || {
    echo "run.sh: \`git\` not on PATH; skipping" >&2
    exit 77
}

LOGS=$(cd .. && pwd)/logs-$NAME
rm -rf "$LOGS"
mkdir -p "$LOGS"

#  Latest sniff baseline row's URI sha (post|get|patch).
head_hex() {
    awk -F'\t' '$2=="post"||$2=="get"||$2=="patch" { last=$3 }
                END {
                    h = last; if (h ~ /#/) sub(/^.*#/, "", h); else sub(/^[^?]*\?/, "", h)
                    if (length(h) == 40 && h ~ /^[0-9a-f]+$/) print h
                }' .be/wtlog
}

# ------------------------------------------------------------------
# 1. Build the bare git repo with the criss-cross DAG via plumbing.
#    Lives in a sibling dir of the wt so it stays out of `be`'s
#    dirty-scan radar.
# ------------------------------------------------------------------
GIT_REPO=$(cd .. && pwd)/crisscross.git
rm -rf "$GIT_REPO"
git init --bare -q "$GIT_REPO"

(
    export GIT_DIR="$GIT_REPO"
    export GIT_AUTHOR_NAME=test     GIT_AUTHOR_EMAIL=test@x
    export GIT_COMMITTER_NAME=test  GIT_COMMITTER_EMAIL=test@x
    export GIT_AUTHOR_DATE='2026-01-01T00:00:00Z'
    export GIT_COMMITTER_DATE='2026-01-01T00:00:00Z'

    A0=$(git hash-object -w "$CASE/01.a-T1.txt")
    B0=$(git hash-object -w "$CASE/02.b-T1.txt")
    C0=$(git hash-object -w "$CASE/03.c-T1.txt")
    A1=$(git hash-object -w "$CASE/04.a-C1.txt")
    B1=$(git hash-object -w "$CASE/05.b-C2.txt")
    B2=$(git hash-object -w "$CASE/06.b-M2.txt")
    C1B=$(git hash-object -w "$CASE/07.c-M1.txt")

    mktree() {
        printf '100644 blob %s\ta.txt\n100644 blob %s\tb.txt\n100644 blob %s\tc.txt\n' \
            "$1" "$2" "$3" | git mktree
    }
    T_T1=$(mktree "$A0" "$B0"  "$C0")
    T_C1=$(mktree "$A1" "$B0"  "$C0")
    T_C2=$(mktree "$A0" "$B1"  "$C0")
    T_M1=$(mktree "$A1" "$B1"  "$C1B")
    T_M2=$(mktree "$A1" "$B2"  "$C0")

    T1=$(echo T1 | git commit-tree "$T_T1")
    C1=$(echo C1 | git commit-tree "$T_C1" -p "$T1")
    C2=$(echo C2 | git commit-tree "$T_C2" -p "$T1")
    M1=$(echo M1 | git commit-tree "$T_M1" -p "$C1" -p "$C2")
    M2=$(echo M2 | git commit-tree "$T_M2" -p "$C2" -p "$C1")

    git update-ref refs/heads/main "$T1"
    git update-ref refs/heads/fix1 "$M1"
    git update-ref refs/heads/fix2 "$M2"
    git symbolic-ref HEAD refs/heads/main

    #  Stash sha map for the run.sh body to read back.
    {
        printf 'T1=%s\n' "$T1"
        printf 'C1=%s\n' "$C1"
        printf 'C2=%s\n' "$C2"
        printf 'M1=%s\n' "$M1"
        printf 'M2=%s\n' "$M2"
    } > "$LOGS/shas"
) > "$LOGS/00.fixture.out" 2> "$LOGS/00.fixture.err"

. "$LOGS/shas"

# ------------------------------------------------------------------
# 2. Import both branches over WIRECLI's git-over-file dispatch.
#    Fetch fix2 FIRST so cur lands on fix2's shard; the subsequent
#    fix1 fetch then targets fix2's shard too (per the unified
#    --at routing: keeper.cli prefers --at branch over URI query
#    for has_authority fetches, and SNIFFAtTailOf forwards the
#    last LOCAL switch as --at).  Both packs end up in fix2/, so
#    PATCH from ?fix1 + POST on fix2 can both resolve M1/M2 via
#    the fix2 shard's keeper-level registry.  After the fix1 fetch
#    cur secretly switches to fix1 (legacy `be get URL?ref`
#    behaviour) so explicitly `be get ?fix2` to re-attach before
#    the PATCH.
# ------------------------------------------------------------------
must "$BE" get "file://$GIT_REPO?fix2" \
    > "$LOGS/01.get-fix2.out" 2> "$LOGS/01.get-fix2.err"
must "$BE" get "file://$GIT_REPO?fix1" \
    > "$LOGS/01a.get-fix1.out" 2> "$LOGS/01a.get-fix1.err"
must "$BE" get '?fix2' \
    > "$LOGS/01b.get-fix2-again.out" 2> "$LOGS/01b.get-fix2-again.err"

#  Pre-merge wt sanity: M2 tree.
match "$CASE/04.a-C1.txt" a.txt
match "$CASE/06.b-M2.txt" b.txt
match "$CASE/03.c-T1.txt" c.txt

GET_TIP=$(head_hex)
[ "$GET_TIP" = "$M2" ] \
    || { echo "imported tip $GET_TIP != M2 $M2" >&2; exit 1; }

# ------------------------------------------------------------------
# 3. `be patch ?fix1` — the criss-cross 3-way merge.
# ------------------------------------------------------------------
sleep 0.2                            # distinct mtime
must "$BE" patch "?fix1" \
    > "$LOGS/02.patch.out" 2> "$LOGS/02.patch.err"

#  No conflict markers anywhere in the wt.
for f in a.txt b.txt c.txt; do
    if grep -E '^(<<<<|>>>>|\|\|\|\|)' "$f" > /dev/null 2>&1; then
        echo "conflict markers leaked into $f after patch:" >&2
        cat "$f" >&2
        exit 1
    fi
done

# ------------------------------------------------------------------
# 4. `be post sync` — single-parent commit on ?fix2 carrying the
#    merged tree.
# ------------------------------------------------------------------
must "$BE" post 'sync msg' \
    > "$LOGS/03.post.out" 2> "$LOGS/03.post.err"

POST_TIP=$(head_hex)
[ -n "$POST_TIP" ] && [ "$POST_TIP" != "$M2" ] \
    || { echo "post didn't advance ?fix2 past M2=$M2 (got $POST_TIP)" >&2
         exit 1; }

#  Single-parent + parent is M2 (PATCH erased provenance of M1).
"$KEEPER" get "?fix2#$POST_TIP" \
    > "$LOGS/04.commit.out" 2> "$LOGS/04.commit.err" \
    || { echo "keeper get .#$POST_TIP failed" >&2
         cat "$LOGS/04.commit.err" >&2; exit 1; }
PARENTS=$(grep -c '^parent ' "$LOGS/04.commit.out" || true)
[ "$PARENTS" = "1" ] \
    || { echo "post-merge tip has $PARENTS parents; want 1" >&2
         cat "$LOGS/04.commit.out" >&2; exit 1; }
PARENT_SHA=$(awk '/^parent / { print $2; exit }' "$LOGS/04.commit.out")
[ "$PARENT_SHA" = "$M2" ] \
    || { echo "post-merge parent=$PARENT_SHA; want M2=$M2" >&2; exit 1; }

# ------------------------------------------------------------------
# 5. Final wt content.
# ------------------------------------------------------------------
match "$CASE/08.a.want.txt" a.txt
match "$CASE/09.b.want.txt" b.txt
match "$CASE/10.c.want.txt" c.txt

#  All assertions passed — drop the logs + fixture on success.
rm -rf "$LOGS" "$GIT_REPO"
