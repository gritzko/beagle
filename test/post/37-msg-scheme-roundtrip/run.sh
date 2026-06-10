#!/bin/sh
#  37-msg-scheme-roundtrip — POST-007 (1): a commit message containing
#  `be://` (or any `scheme://` / doubled `//`) must round-trip
#  BYTE-IDENTICAL.  A commit message is opaque free text; the doubled
#  slash must NOT collapse (`be://` stored/displayed as `be:/`).
#
#  The bug: the message slot was run through a `//`-collapsing path
#  normalization (the fragment-vs-path argv classification), so any
#  `scheme://` in a subject was corrupted.  POST-002 routes a `#`-led
#  body into the fragment slot verbatim (no path normalization); this
#  case is the end-to-end guard for the `be://` repro from the ticket.
#
#  Asserts, for each message form (explicit `#`, whitespace-bypass,
#  legacy `-m`):
#    1. `be post` commits (not refused as a path-form URI).
#    2. The stored commit subject contains `be://` VERBATIM — never
#       `be:/`.

. "$(dirname "$0")/../../lib/branches.sh"

#  Read the committed subject (first body line, after the header/body
#  blank line) for the tip recorded in the wtlog.
committed_subject() {
    _h=$1
    "$KEEPER" get ".#$_h" 2>/dev/null \
        | awk '/^$/ { p = 1; next } p { print; exit }'
}

assert_roundtrip() {
    _label=$1; _stored=$2
    case "$_stored" in
        *be://*) ;;   # good — doubled slash preserved
        *be:/*)
            fail "$_label: '//' collapsed — stored '$_stored' (be:// → be:/)" ;;
        *)
            fail "$_label: be:// missing from stored subject '$_stored'" ;;
    esac
    note "$_label: subject round-trips verbatim — '$_stored'"
}

cp "$CASE/01.greet.txt" greet.txt
"$BE" put greet.txt >/dev/null

# --- 1. explicit `#`-led message ------------------------------------
"$BE" post '#fix the be:// scheme' >"$ETMP/p1.out" 2>"$ETMP/p1.err" \
    || fail "be post '#fix the be:// scheme' refused; err: $(cat $ETMP/p1.err)"
grep -qi 'path-form URI' "$ETMP/p1.err" \
    && fail "post wrongly refused be:// message as path-form: $(cat $ETMP/p1.err)"
H1=$(head_hex)
[ -n "$H1" ] || fail "explicit-# post did not create a commit"
S1=$(committed_subject "$H1")
[ "$S1" = "fix the be:// scheme" ] \
    || fail "explicit-#: subject should be 'fix the be:// scheme', got '$S1'"
assert_roundtrip "explicit-#" "$S1"

# --- 2. whitespace-bypass message (no leading `#`) ------------------
printf 'change a\n' >>greet.txt
"$BE" put greet.txt >/dev/null
"$BE" post 'fix the be:// scheme again' >"$ETMP/p2.out" 2>"$ETMP/p2.err" \
    || fail "be post 'fix the be:// scheme again' refused; err: $(cat $ETMP/p2.err)"
H2=$(head_hex)
[ -n "$H2" ] && [ "$H2" != "$H1" ] \
    || fail "whitespace-bypass post did not create a new commit (H2=$H2)"
S2=$(committed_subject "$H2")
[ "$S2" = "fix the be:// scheme again" ] \
    || fail "whitespace: subject should be 'fix the be:// scheme again', got '$S2'"
assert_roundtrip "whitespace" "$S2"

# --- 3. legacy `-m` message ----------------------------------------
printf 'change b\n' >>greet.txt
"$BE" put greet.txt >/dev/null
"$BE" post -m 'use be://localhost/store' >"$ETMP/p3.out" 2>"$ETMP/p3.err" \
    || fail "be post -m 'use be://localhost/store' refused; err: $(cat $ETMP/p3.err)"
H3=$(head_hex)
[ -n "$H3" ] && [ "$H3" != "$H2" ] \
    || fail "-m post did not create a new commit (H3=$H3)"
S3=$(committed_subject "$H3")
[ "$S3" = "use be://localhost/store" ] \
    || fail "-m: subject should be 'use be://localhost/store', got '$S3'"
assert_roundtrip "-m" "$S3"

note "POST-007 (1): be:// round-trips byte-identical in commit messages"
echo "=== post/37-msg-scheme-roundtrip: OK ==="
