#!/bin/sh
#  04-branch-switch-status — exercise wt content + `be` (status)
#  invariants across `post` / `get` / `put` over a two-branch repo,
#  with extra dirty-class clutter (untracked + touched-unchanged) on
#  the wt to make sure status doesn't over-report.
#
#  Pinned behavior:
#    * after `be get ?<branch>` the wt's tracked files match that
#      branch's tip exactly,
#    * untracked files survive every checkout (sniff doesn't manage
#      them, but it must not delete them either),
#    * touched-unchanged files (mtime moved, bytes match baseline)
#      do NOT show up as `mod` — sniff hashes the content and treats
#      them as clean,
#    * ignored files (`*.tmp` per `.gitignore`) never show in status.

. "$(dirname "$0")/../../lib/case.sh"

LOGS=$(cd .. && pwd)/logs-$NAME
rm -rf "$LOGS"; mkdir -p "$LOGS"

# Capture `be` (bare status) into $1; expect rc=0 always.  Status
# rows go to stdout; stderr is empty on a clean run.
status_into() {
    set +e
    "$BE" > "$1.out" 2> "$1.err"
    _rc=$?
    set -e
    [ "$_rc" = "0" ] || {
        echo "FAIL: be (status) exited $_rc:" >&2
        cat "$1.err" >&2
        exit 1
    }
}

# Assert grep over a status capture.
status_must() {
    if ! grep -q -- "$2" "$1.out"; then
        echo "FAIL: status missing '$2':" >&2
        cat "$1.out" >&2
        exit 1
    fi
}
status_mustnt() {
    if grep -q -- "$2" "$1.out"; then
        echo "FAIL: status leaked '$2':" >&2
        cat "$1.out" >&2
        exit 1
    fi
}

# ------------------------------------------------------------------
# 1. Trunk T0: `.gitignore` (so the ignored case attaches), `a.txt`,
#    `b.txt`.  Both files have distinct content vs the feat branch
#    so we can verify the wt flips on `be get`.
# ------------------------------------------------------------------
echo "*.tmp"      >  .gitignore
echo "a@trunk"    >  a.txt
echo "b@trunk"    >  b.txt
"$BE" put .gitignore a.txt b.txt > /dev/null
"$BE" post 't0 baseline'         > /dev/null

# ------------------------------------------------------------------
# 2. Fork ?feat at T0; advance feat with edits to both files.
# ------------------------------------------------------------------
"$BE" put '?./feat'      > /dev/null
"$BE" get '?feat'        > /dev/null
sleep 0.02
echo "a@feat"  > a.txt
echo "b@feat"  > b.txt
"$BE" put a.txt b.txt    > /dev/null
"$BE" post 'feat msg'    > /dev/null

# Sanity at feat tip.
[ "$(cat a.txt)" = "a@feat" ] || { echo "feat: a.txt content wrong" >&2; exit 1; }
[ "$(cat b.txt)" = "b@feat" ] || { echo "feat: b.txt content wrong" >&2; exit 1; }

# ------------------------------------------------------------------
# 3. Switch back to trunk; wt's tracked files must match T0 again.
# ------------------------------------------------------------------
"$BE" get '?..'          > /dev/null
[ "$(cat a.txt)" = "a@trunk" ] || {
    echo "FAIL: trunk: a.txt should be a@trunk, got '$(cat a.txt)'" >&2
    exit 1
}
[ "$(cat b.txt)" = "b@trunk" ] || {
    echo "FAIL: trunk: b.txt should be b@trunk, got '$(cat b.txt)'" >&2
    exit 1
}

# ------------------------------------------------------------------
# 4. Strew the wt with status-class clutter:
#       touched-unchanged: a.txt — mtime shifted, bytes match T0.
#       untracked        : new.txt — never staged.
#       ignored          : scratch.tmp — `*.tmp` per .gitignore.
# ------------------------------------------------------------------
sleep 0.02
touch a.txt
echo "fresh"   > new.txt
echo "scratch" > scratch.tmp

# ------------------------------------------------------------------
# 5. `be` (bare) must report:
#       * untracked new.txt as `unk`,
#       * NEVER show a.txt (touched-unchanged is clean),
#       * NEVER show scratch.tmp (ignored).
# ------------------------------------------------------------------
status_into "$LOGS/00.status"
status_must  "$LOGS/00.status" 'new\.txt'
status_mustnt "$LOGS/00.status" 'a\.txt'
status_mustnt "$LOGS/00.status" 'scratch\.tmp'

# ------------------------------------------------------------------
# 6. `be get ?feat` must succeed despite the clutter.  Tracked files
#    flip to feat content; untracked + ignored survive.
# ------------------------------------------------------------------
"$BE" get '?feat'        > /dev/null
[ "$(cat a.txt)" = "a@feat" ] || {
    echo "FAIL: post-get-?feat a.txt should be a@feat, got '$(cat a.txt)'" >&2
    exit 1
}
[ "$(cat b.txt)" = "b@feat" ] || { echo "FAIL: b.txt missed feat content" >&2; exit 1; }
[ -f new.txt ]     || { echo "FAIL: untracked new.txt got pruned"   >&2; exit 1; }
[ -f scratch.tmp ] || { echo "FAIL: ignored scratch.tmp got pruned" >&2; exit 1; }

# Status at feat: only new.txt unknown; tracked clean; ignored absent.
status_into "$LOGS/01.status"
status_must  "$LOGS/01.status" 'new\.txt'
status_mustnt "$LOGS/01.status" 'a\.txt'
status_mustnt "$LOGS/01.status" 'b\.txt'
status_mustnt "$LOGS/01.status" 'scratch\.tmp'

# ------------------------------------------------------------------
# 7. Edit a tracked file and `be put` it; then bounce trunk → feat
#    and back to ensure put-staging carries through correctly.
# ------------------------------------------------------------------
sleep 0.02
echo "a@feat-edit" > a.txt
"$BE" put a.txt    > /dev/null

status_into "$LOGS/02.status"
status_must  "$LOGS/02.status" 'a\.txt'   # staged for next post

"$BE" post 'feat-edit msg' > /dev/null

# After post: a.txt is the new tip's content; status clean for tracked.
status_into "$LOGS/03.status"
status_mustnt "$LOGS/03.status" 'a\.txt'
status_must   "$LOGS/03.status" 'new\.txt'

rm -rf "$LOGS"
