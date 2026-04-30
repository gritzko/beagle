#!/bin/sh
#  branches/06-post-shapes — extracted from workflow-branches.sh stage 28.
#  POSTNONE / selective / implicit baseline shapes on trunk.

. "$(dirname "$0")/../../lib/branches.sh"
WT="$SCRATCH"

# --- setup: trunk with seed commit ---
echo "x v1" > x.txt
"$BE" post v1 >/dev/null

echo "=== 28. baseline POST shapes ==="

# 28a — bare POST with no changes → POSTNONE
echo "=== 28a. POST on clean wt → POSTNONE ==="
T28a_pre=$(ref_tip "?")
[ -n "$T28a_pre" ] || fail "§28a: no trunk tip"
set +e
"$BE" post 28a-msg 2>"$ETMP/p28a.err" >/dev/null
EC=$?
set -e
[ "$EC" != "0" ] || fail "§28a: POST with msg + no changes should fail"
grep -q 'no changes since base' "$ETMP/p28a.err" \
    || fail "§28a: stderr should mention 'no changes since base'"
T28a_post=$(ref_tip "?")
[ "$T28a_post" = "$T28a_pre" ] \
    || fail "§28a: trunk REFS moved on POSTNONE"
note "§28a OK: bare POST refused; trunk unchanged"

# 28b — selective: edit two files, put only one, post.
echo "=== 28b. selective mode (be put a.txt; be post msg) ==="
sleep 0.01
echo "a v28b" > a28.txt
echo "b v28b" > b28.txt
"$BE" put a28.txt b28.txt >/dev/null \
    || fail "§28b: stage initial baseline failed"
"$BE" post 28b-base >/dev/null \
    || fail "§28b: baseline commit failed"
T28b_base=$(head_hex)

B28_BASE_BLOB=$("$KEEPER" ls-files ".#$T28b_base" 2>/dev/null \
                  | awk '$NF=="b28.txt"{print $3; exit}')
[ -n "$B28_BASE_BLOB" ] || fail "§28b: missing b28.txt baseline blob"

sleep 0.01
echo "a edited 28b" > a28.txt
echo "b edited 28b" > b28.txt
"$BE" put a28.txt >/dev/null \
    || fail "§28b: be put a28.txt failed"
"$BE" post 28b-selective >/dev/null \
    || fail "§28b: be post 28b-selective failed"
T28b_sel=$(head_hex)
[ "$T28b_sel" != "$T28b_base" ] || fail "§28b: tip didn't advance"

A_NEW=$("$KEEPER" ls-files ".#$T28b_sel" 2>/dev/null \
          | awk '$NF=="a28.txt"{print $3; exit}')
B_NEW=$("$KEEPER" ls-files ".#$T28b_sel" 2>/dev/null \
          | awk '$NF=="b28.txt"{print $3; exit}')
[ -n "$A_NEW" ] && [ -n "$B_NEW" ] || fail "§28b: missing blobs in new commit"
[ "$B_NEW" = "$B28_BASE_BLOB" ] \
    || fail "§28b: b28.txt should carry over baseline blob"
grep -qx 'b edited 28b' b28.txt \
    || fail "§28b: wt b28.txt should still hold edited bytes"
note "§28b OK: a28 rewrote, b28 carried over, wt b28 still edited"

# 28c — implicit mode: edit two files, post (no puts).
echo "=== 28c. implicit mode (no puts; bare-msg POST) ==="
sleep 0.01
echo "a v28c base" > a28c.txt
echo "b v28c base" > b28c.txt
"$BE" put a28c.txt b28c.txt >/dev/null \
    || fail "§28c: stage initial baseline failed"
"$BE" post 28c-base >/dev/null \
    || fail "§28c: baseline commit failed"
T28c_base=$(head_hex)
B28C_BASE_BLOB=$("$KEEPER" ls-files ".#$T28c_base" 2>/dev/null \
                  | awk '$NF=="b28c.txt"{print $3; exit}')

sleep 0.01
echo "a edited 28c" > a28c.txt
echo "b edited 28c" > b28c.txt
"$BE" post 28c-implicit >/dev/null \
    || fail "§28c: be post 28c-implicit failed"
T28c_imp=$(head_hex)
[ "$T28c_imp" != "$T28c_base" ] || fail "§28c: tip didn't advance"

A2=$("$KEEPER" ls-files ".#$T28c_imp" 2>/dev/null \
       | awk '$NF=="a28c.txt"{print $3; exit}')
B2=$("$KEEPER" ls-files ".#$T28c_imp" 2>/dev/null \
       | awk '$NF=="b28c.txt"{print $3; exit}')
[ -n "$A2" ] && [ -n "$B2" ] || fail "§28c: missing blobs in implicit commit"
[ "$B2" != "$B28C_BASE_BLOB" ] \
    || fail "§28c: b28c.txt should be rewritten in implicit mode"
note "§28c OK: implicit commit rewrote both files"

echo "=== branches/06-post-shapes: OK ==="
