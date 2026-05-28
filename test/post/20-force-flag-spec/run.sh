#!/bin/sh
#  post/20-force-flag-spec — `be post --force "msg"` must either
#  reject the flag at the dispatcher OR be defined in VERBS.md.
#
#  Spec (VERBS.md):
#    §GET defines `--force` (overwrites dirty tracked paths, bypasses
#    no-baseline overlay refusal).
#    §POST does NOT mention `--force`.
#
#  Today the flag is silently parsed and forwarded — sniff/POST.c
#  uses it to bypass the conflict-marker refusal at POSTCFLCT.  Per
#  the originating report:
#    > `be post --force` accepted without spec coverage … unclear
#    > what it disabled (FF check? dirty check?).  Either define it
#    > or refuse the flag at the dispatcher.
#
#  This test pins the "refuse" arm: on a clean wt with no conflict
#  markers, `be post --force "msg"` must surface a "--force not
#  supported for post" diagnostic and non-zero exit.  Flip the test
#  (or delete it) once VERBS.md §POST documents the semantics.

. "$(dirname "$0")/../../lib/case.sh"

"$BE" put "?/$P/" 2>/dev/null || true

LOGS="$SCRATCH/logs"; mkdir -p "$LOGS"

# 1. baseline commit so the wt has a parent to post against.
echo "v1" > x.txt
"$BE" put x.txt   > "$LOGS/01.put.out" 2>&1
"$BE" post 'baseline' > "$LOGS/02.post.out" 2>&1

# 2. tracked edit (so post has something to do; no patch row, no
#    conflict marker).
sleep 0.02
echo "v2" > x.txt

# 3. `be post --force 'msg'` on a clean conflict-free wt.  Per spec
#    --force is undefined for POST, so the dispatcher must refuse.
set +e
"$BE" post --force 'msg with force' \
    > "$LOGS/03.force.out" 2> "$LOGS/03.force.err"
RC=$?
set -e

if [ "$RC" -eq 0 ]; then
    echo "FAIL: be post --force silently accepted; spec defines --force only for GET" >&2
    echo "      stdout:" >&2
    sed 's/^/        /' "$LOGS/03.force.out" >&2
    echo "      stderr:" >&2
    sed 's/^/        /' "$LOGS/03.force.err" >&2
    exit 1
fi

#  A useful refusal message names the verb and the flag.
grep -q -i 'force' "$LOGS/03.force.err" || {
    echo "FAIL: expected '--force' mentioned in refusal stderr; got:" >&2
    cat "$LOGS/03.force.err" >&2
    exit 1
}

rm -rf "$LOGS"
