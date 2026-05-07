#!/bin/sh
#  07-patch-multi-author — bare `be post` after multiple cherry-picks
#  must inherit message + author from the topologically latest patched
#  commit, with " (+N)" / " (et al)" decorations.
#
#  Branch shape:
#
#      trunk ─ T0   ←── cur on POST
#               \
#               fix1: F1 (Alice) ─ F2 (Bob)
#
#  F1 (Alice) edits lib.c (block + line) and adds hello.c.
#  F2 (Bob)  edits util.c (token-level literal swap) and deletes bye.txt.
#  Disjoint file sets — required because PATCH-on-PATCH refuses
#  overlapping files until the WEAVE composition TODO in PATCH.c lands.
#
#  After two cherry-picks (`be patch #F1`, `be patch #F2`) onto trunk's
#  wt, bare `be post` (no -m) must produce ONE single-parent commit
#  with:
#    * message  = "f2 bob changes (+1)"   ← F2's body + 1 extra patch
#    * author   = "Bob (et al) <bob@example.com> ..."
#                                         ← F2's author + et al because
#                                           F1's author (Alice) differs
#
#  The `.dogs/config` is rewritten to "Carol" before POST so the
#  inherited identity provably comes from the patch rows, not from
#  the live config.

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
    mkdir -p .dogs
    cat > .dogs/config <<EOF
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
                }' .sniff
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
# 5. Back to trunk; rewrite config to Carol so the inherited author
#    provably comes from the patch rows, not the live config.
# ------------------------------------------------------------------
"$BE" get '?..'                  > /dev/null
set_author "Carol" "carol@example.com"

# ------------------------------------------------------------------
# 6. Two cherry-picks: F1 then F2.  Files touched are disjoint, so
#    PATCH's dirty refusal does not fire.
# ------------------------------------------------------------------
"$BE" patch "#$F1" > "$LOGS/p1.out" 2> "$LOGS/p1.err"
"$BE" patch "#$F2" > "$LOGS/p2.out" 2> "$LOGS/p2.err"

# Post-cherry-pick wt content: union of F1 and F2's edits.
match "$CASE/04.lib.f1.c"   lib.c
match "$CASE/06.util.f2.c"  util.c
match "$CASE/05.hello.f1.c" hello.c
[ ! -e bye.txt ] || { echo "FAIL: bye.txt should be deleted" >&2; exit 1; }

# ------------------------------------------------------------------
# 7. Bare `be post` — defaults must be composed from the patch rows.
# ------------------------------------------------------------------
"$BE" post > "$LOGS/post.out" 2> "$LOGS/post.err"

POST_TIP=$(head_hex)
[ -n "$POST_TIP" ] && [ "$POST_TIP" != "$F2" ] \
    || { echo "post didn't advance trunk past F2=$F2 (got $POST_TIP)" >&2
         exit 1; }

"$KEEPER" get ".#$POST_TIP" \
    > "$LOGS/commit.out" 2> "$LOGS/commit.err" \
    || { echo "keeper get .#$POST_TIP failed" >&2
         cat "$LOGS/commit.err" >&2; exit 1; }

#  Author: F2's author with "(et al)" injected before the email
#  because F1's author (Alice) differs from F2's (Bob).  Live config
#  was Carol — must be ignored.
grep -q '^author Bob (et al) <bob@example.com> ' "$LOGS/commit.out" \
    || { echo "FAIL: expected author 'Bob (et al) <bob@example.com>'" >&2
         grep '^author' "$LOGS/commit.out" >&2; exit 1; }
grep -q 'Carol\|carol@' "$LOGS/commit.out" \
    && { echo "FAIL: live config (Carol) leaked into commit" >&2
         cat "$LOGS/commit.out" >&2; exit 1; }

#  Subject (first body line): F2's message + " (+1)".  The 1 is the
#  count of additional absorbed patches beyond the chosen one (F1).
SUBJECT=$(awk '/^$/{p=1; next} p { print; exit }' "$LOGS/commit.out")
[ "$SUBJECT" = "f2 bob changes (+1)" ] \
    || { echo "FAIL: expected subject 'f2 bob changes (+1)', got '$SUBJECT'" >&2
         exit 1; }

#  Single-parent invariant.
PARENTS=$(grep -c '^parent ' "$LOGS/commit.out" || true)
[ "$PARENTS" = "1" ] \
    || { echo "post-merge tip has $PARENTS parents; want 1" >&2
         exit 1; }

rm -rf "$LOGS"
