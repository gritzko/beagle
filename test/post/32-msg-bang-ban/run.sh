#!/bin/sh
#  32-msg-bang-ban — DIS-031: a commit MESSAGE may not end in `!`.
#  The trailing `!` on a post fragment is the forget modifier, so a
#  literal message ending in `!` would silently become a forget.  POST
#  refuses it with the dedicated POSTBANG error.
#
#    `be post '#fix it!'`   → modifier=forget, message="fix it"  (OK)
#    `be post '#fix it!!'`  → modifier=forget, message="fix it!" (BAN)
#
#  Uses the no-patch-row path: a plain staged file, no foster source,
#  so `#fix it!` resolves to (new msg "fix it", forget) with no foster
#  target — the commit still lands; only the message-ending-`!` ban
#  must fire.

. "$(dirname "$0")/../../lib/branches.sh"

cp "$CASE/01.greet.txt" greet.txt
"$BE" put greet.txt >/dev/null

# `#fix it!!` — after stripping the trailing `!` modifier, the message
# "fix it!" still ends in `!` → POSTBANG, refused before any pack write.
"$BE" post '#fix it!!' >"$ETMP/ban.out" 2>"$ETMP/ban.err" \
    && fail "be post '#fix it!!' must be refused (message ends in '!')"
grep -qi 'POSTBANG\|message.*may not end\|ends in' "$ETMP/ban.err" \
    || fail "expected POSTBANG/ends-in-! diagnostic; got: $(cat $ETMP/ban.err)"

# The refused post must NOT have advanced cur (no commit).
H1=$(head_hex)
[ -z "$H1" ] || fail "refused post still created a commit ($H1)"

# `#fix it!` — trailing `!` is the (no-op here) forget modifier; the
# real message "fix it" does NOT end in `!`, so the commit lands.
"$BE" post '#fix it!' >"$ETMP/ok.out" 2>"$ETMP/ok.err" \
    || fail "be post '#fix it!' should commit (msg 'fix it'); err: $(cat $ETMP/ok.err)"
H2=$(head_hex)
[ -n "$H2" ] || fail "be post '#fix it!' did not create a commit"
BODY=$("$KEEPER" get ".#$H2" 2>/dev/null) || fail "keeper get .#$H2 failed"
echo "$BODY" | grep -q '^fix it$' || fail "message should be exactly 'fix it' (trailing ! stripped)"

note "msg-bang-ban OK: '#fix it!!' refused (POSTBANG); '#fix it!' → msg 'fix it'"
echo "=== post/32-msg-bang-ban: OK ==="
