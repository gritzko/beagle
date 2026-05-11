#!/bin/sh
#  07-patch-multi-author — two cherry-picks from different authors,
#  followed by POST.  Per the new spec (VERBS.md §POST "Message
#  resolution"), bare `be post` with >1 applied commit is AMBIGUOUS
#  and must refuse with POSTNOMSG; the user supplies an explicit
#  msg.  The resulting commit carries `picked: <sha>` trailers for
#  every cherry-picked source (no DAG edge, just provenance).
#  Author comes from the live `.be/config` — no inheritance from
#  patch rows in the new spec.
#
#  Branch shape:
#
#      trunk ─ T0   ←── cur on POST
#               \
#               fix1: F1 (Alice) ─ F2 (Bob)
#
#  F1 (Alice) edits lib.c (block + line) and adds hello.c.
#  F2 (Bob)  edits util.c (token-level literal swap) and deletes bye.txt.

. "$(dirname "$0")/../../lib/case.sh"

KEEPER=${KEEPER:-${BIN:+$BIN/keeper}}
KEEPER=${KEEPER:-$(command -v keeper || true)}
[ -n "$KEEPER" ] && [ -x "$KEEPER" ] || {
    echo "run.sh: cannot locate \`keeper\`" >&2
    exit 2
}

LOGS=$(cd .. && pwd)/logs-$NAME
rm -rf "$LOGS"; mkdir -p "$LOGS"

set_author() {
    mkdir -p .be
    cat > .be/config <<EOF
[user]
name = "$1"
email = "$2"
EOF
}

# Latest sniff baseline row's URI sha (post|get|patch).
head_hex() {
    awk -F'\t' '$2=="post"||$2=="get"||$2=="patch" { last=$3 }
                END {
                    h = last; sub(/^[^#]*#/, "", h)
                    if (length(h) == 40 && h ~ /^[0-9a-f]+$/) print h
                }' .be/wtlog
}

# ------------------------------------------------------------------
# 1. T0 baseline on trunk, author Alice.
# ------------------------------------------------------------------
set_author "Alice" "alice@example.com"
sleep 0.02
cp "$CASE/01.lib.t0.c"  lib.c
cp "$CASE/02.util.t0.c" util.c
cp "$CASE/03.bye.txt"   bye.txt
"$BE" put lib.c util.c bye.txt > /dev/null
"$BE" post 't0 baseline'        > /dev/null

# ------------------------------------------------------------------
# 2. Fork fix1 and switch to it.
# ------------------------------------------------------------------
"$BE" put '?./fix1'             > /dev/null
"$BE" get '?fix1'               > /dev/null

# ------------------------------------------------------------------
# 3. F1 — Alice: block + line edits to lib.c, add hello.c.
# ------------------------------------------------------------------
sleep 0.02
cp "$CASE/04.lib.f1.c"   lib.c
cp "$CASE/05.hello.f1.c" hello.c
"$BE" put lib.c hello.c          > /dev/null
"$BE" post 'f1 alice changes'    > /dev/null
F1=$(head_hex)
[ -n "$F1" ] || { echo "F1 sha missing" >&2; exit 1; }

# ------------------------------------------------------------------
# 4. F2 — Bob: token edit on util.c, delete bye.txt.
# ------------------------------------------------------------------
set_author "Bob" "bob@example.com"
sleep 0.02
cp "$CASE/06.util.f2.c" util.c
"$BE" put    util.c              > /dev/null
"$BE" delete bye.txt             > /dev/null
"$BE" post   'f2 bob changes'    > /dev/null
F2=$(head_hex)
[ -n "$F2" ] || { echo "F2 sha missing" >&2; exit 1; }

# ------------------------------------------------------------------
# 5. Back to trunk; rewrite config to Carol.
# ------------------------------------------------------------------
"$BE" get '?..'                  > /dev/null
set_author "Carol" "carol@example.com"

# ------------------------------------------------------------------
# 6. Two cherry-picks: F1 then F2.
# ------------------------------------------------------------------
"$BE" patch "#$F1" > "$LOGS/p1.out" 2> "$LOGS/p1.err"
"$BE" patch "#$F2" > "$LOGS/p2.out" 2> "$LOGS/p2.err"

# Post-cherry-pick wt content: union of F1 and F2's edits.
match "$CASE/04.lib.f1.c"   lib.c
match "$CASE/06.util.f2.c"  util.c
match "$CASE/05.hello.f1.c" hello.c
[ ! -e bye.txt ] || { echo "FAIL: bye.txt should be deleted" >&2; exit 1; }

# ------------------------------------------------------------------
# 7. Bare `be post` — ambiguous (2 applied commits with usable msgs);
#    must refuse with POSTNOMSG.
# ------------------------------------------------------------------
if "$BE" post > "$LOGS/post1.out" 2> "$LOGS/post1.err"; then
    echo "FAIL: bare post should have refused with POSTNOMSG" >&2
    cat "$LOGS/post1.err" >&2
    exit 1
fi
grep -q 'cannot auto-resolve commit msg' "$LOGS/post1.err" \
    || { echo "FAIL: expected POSTNOMSG message" >&2
         cat "$LOGS/post1.err" >&2; exit 1; }

# ------------------------------------------------------------------
# 8. Explicit msg POST — succeeds.  Commit must carry picked: F1
#    and picked: F2 trailers.  Author from live config (Carol).
# ------------------------------------------------------------------
"$BE" post '#cherry-pick F1+F2' > "$LOGS/post.out" 2> "$LOGS/post.err"

POST_TIP=$(head_hex)
[ -n "$POST_TIP" ] && [ "$POST_TIP" != "$F2" ] \
    || { echo "post didn't advance trunk past F2=$F2 (got $POST_TIP)" >&2
         exit 1; }

"$KEEPER" get ".#$POST_TIP" \
    > "$LOGS/commit.out" 2> "$LOGS/commit.err" \
    || { echo "keeper get .#$POST_TIP failed" >&2
         cat "$LOGS/commit.err" >&2; exit 1; }

#  Author: Carol (current config).  No inheritance from patch rows.
grep -q '^author Carol <carol@example.com> ' "$LOGS/commit.out" \
    || { echo "FAIL: expected author 'Carol <carol@example.com>'" >&2
         grep '^author' "$LOGS/commit.out" >&2; exit 1; }

#  Subject: explicit POST msg.
SUBJECT=$(awk '/^$/{p=1; next} p { print; exit }' "$LOGS/commit.out")
[ "$SUBJECT" = "cherry-pick F1+F2" ] \
    || { echo "FAIL: expected subject 'cherry-pick F1+F2', got '$SUBJECT'" >&2
         exit 1; }

#  picked: F1 and picked: F2 trailers.
grep -q "^picked: $F1\$" "$LOGS/commit.out" \
    || { echo "FAIL: missing 'picked: $F1' trailer" >&2
         cat "$LOGS/commit.out" >&2; exit 1; }
grep -q "^picked: $F2\$" "$LOGS/commit.out" \
    || { echo "FAIL: missing 'picked: $F2' trailer" >&2
         cat "$LOGS/commit.out" >&2; exit 1; }

#  First-parent linearity: exactly one `parent` line (cur's prior
#  tip = T0).  Cherry-picks contribute trailers, not parents.
PARENTS=$(grep -c '^parent ' "$LOGS/commit.out" || true)
[ "$PARENTS" = "1" ] \
    || { echo "post-merge tip has $PARENTS parents; want 1" >&2
         cat "$LOGS/commit.out" >&2
         exit 1; }

rm -rf "$LOGS"
