#!/bin/sh
#  16-conflict-loud — when WEAVE cannot auto-resolve, PATCH must:
#    1. write the file with 4-char `<<<<` / `>>>>` markers,
#    2. emit a `patch conf <path>` row (bright red),
#    3. DIS-018: return OK (exit 0) — a non-zero exit broke parent
#       recursion on a conflicting submodule; the markered file is the
#       POST-time safety net (POSTCFLCT) instead.
#
#  Topology: trunk T1 and feat F1 both edit the SAME line of lib.c.
#       T0 ── T1   ← cur (trunk)  greet = "hello"
#         \
#          F1      ← ?feat        greet = "salaam"

. "$(dirname "$0")/../../lib/branches.sh"

cp "$CASE/01.lib.t0.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't0' >/dev/null
T0=$(head_hex)

"$BE" put '?./feat' >/dev/null

# Trunk T1: greet=hello.
sleep 0.02; cp "$CASE/02.lib.t1.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 't1 greet=hello' >/dev/null
T1=$(head_hex)

# Feat F1: greet=salaam (same line as T1 → conflict).
"$BE" get '?feat' >/dev/null
sleep 0.02; cp "$CASE/03.lib.f1.c" lib.c
"$BE" put lib.c >/dev/null
"$BE" post 'f1 greet=salaam' >/dev/null
F1=$(head_hex)

"$BE" get '?..' >/dev/null

# THE ACTION: merge feat into trunk — `?feat!` whole-branch scope —
# conflicts, but DIS-018 says PATCH returns OK (exit 0) and reports
# `conf`; markers stay in the file.
"$BE" patch '?feat!' >"$ETMP/c.out" 2>"$ETMP/c.err" \
    || fail "be patch '?feat!' should exit 0 now (DIS-018); err: $(cat $ETMP/c.err)"

# Per-file status row: lib.c → conf (DIS-018, was `conflict`).
grep -E '[[:space:]]+conf[[:space:]]+(\./)?lib\.c$' "$ETMP/c.out" \
    || fail "expected 'patch conf lib.c' status row; got: $(cat $ETMP/c.err)"

# Wt must contain token-level 4-char markers (inline).  Format:
# `>>>>theirs||||ours<<<<` — graf's WEAVE engine marks the
# divergence span with these around the conflicting tokens.
grep -F '<<<<' lib.c \
    || fail "expected '<<<<' marker in lib.c after conflict; lib.c is: $(cat lib.c)"
grep -F '>>>>' lib.c \
    || fail "expected '>>>>' marker in lib.c after conflict"
grep -F '||||' lib.c \
    || fail "expected '||||' separator between theirs/ours sides"
# Must NOT use git's 7-char markers.
grep -F '<<<<<<<' lib.c \
    && fail "found 7-char git-style marker; spec says token-level 4-char"

# POST must be refused — conflict markers in tracked file.
mustnt "$BE" post '#try anyway'

note "conflict-loud OK: lib.c has 4-char markers; patch reported conf, exit 0; POST refused"
echo "=== patch/16-conflict-loud: OK ==="
