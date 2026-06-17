#!/bin/sh
#  get/52-default-state-banner — GET-026 / BE-005.
#
#  Bare `be get` (no URI) and `be get ?` are the DEFAULT GET: they run
#  the normal checkout/update path and report the RESULTING STATE
#  through the shared ROWS state banner — a pale-yellow `get ?#<hashlet>`
#  hunk header — NOT the old raw `branches:`/`remotes:` summary, and NOT
#  silence on an up-to-date worktree.  Table-driven over (mode, lag):
#
#    up-to-date wt, --color : banner band present, no file rows
#    up-to-date wt, --tlv   : H-hunk carries the `?#<hashlet>` uri
#    lagging   wt, --color  : banner band + an `a.txt` file row (FF)
#    lagging   wt, --tlv    : H-hunk uri + the `a.txt` row text (FF)
#
#  The lag is a second worktree sharing the first's store: wt A advances
#  the branch tip while wt B stays behind, so `be get` in B fast-forwards
#  and streams the crossed-commit + per-file rows under the banner.

. "$(dirname "$0")/../../lib/case.sh"

ROOT=$SCRATCH

# Pale-yellow ROWS/BRO state band marker (THEME_BANNER bg).
BAND='48;5;230m'

# banner_must FILE LABEL — assert a state-banner line `get ?...#<hashlet>`
# painted in the pale-yellow band is present in a --color capture.
banner_must() {
    if ! grep -aq -- "${BAND}.*get ?" "$1"; then
        echo "FAIL ($2): no pale-yellow 'get ?...' state banner in:" >&2
        cat -v "$1" >&2
        exit 1
    fi
}

# uri_must FILE LABEL — assert the `?...#<8hex>` state uri is present in a
# --tlv capture (the H-hunk's U-field; -a so binary doesn't muzzle grep).
uri_must() {
    if ! grep -aqE -- '[?][a-z0-9]*#[0-9a-f]{8}' "$1"; then
        echo "FAIL ($2): no '?...#<hashlet>' state uri in --tlv stream:" >&2
        cat -v "$1" | head -c 400 >&2; echo >&2
        exit 1
    fi
}

# row_must FILE LABEL PAT — assert a per-file row matching PAT is present.
row_must() {
    if ! grep -aq -- "$3" "$1"; then
        echo "FAIL ($2): expected a '$3' file row under the banner:" >&2
        cat -v "$1" >&2
        exit 1
    fi
}

# ------------------------------------------------------------------
# wt A: one commit (c1) on trunk.
# ------------------------------------------------------------------
cd "$ROOT"
echo v1 > a.txt
"$BE" put a.txt           > /dev/null 2>&1
"$BE" post 'c1'           > /dev/null 2>&1

# ------------------------------------------------------------------
# UP-TO-DATE wt (A is at the tip).  Bare `be get` and `be get ?` both
# print the state banner; neither prints a file row (nothing changed).
# ------------------------------------------------------------------
"$BE" get --color    > up.bare.out  2> up.bare.err
banner_must up.bare.out "bare/up-to-date/color"

"$BE" get '?' --color > up.q.out    2> up.q.err
banner_must up.q.out    "?/up-to-date/color"

"$BE" get '?' --tlv  > up.q.tlv     2> up.q.tlv.err
uri_must up.q.tlv       "?/up-to-date/tlv"

# The legacy raw summary must be GONE from the default get.
if grep -aq -- 'branches:' up.bare.out || grep -aq -- 'remotes:' up.bare.out; then
    echo "FAIL: default get still prints the raw branches/remotes summary:" >&2
    cat -v up.bare.out >&2
    exit 1
fi

# ------------------------------------------------------------------
# LAGGING wt: a second worktree B shares A's store, clones at c1, then
# A advances to c2.  B's bare `be get` must FF to c2, streaming the
# crossed-commit + per-file rows beneath the state banner.
# ------------------------------------------------------------------
cd "$ROOT"
mkdir B
( cd B && "$BE" get "file:$ROOT" > /dev/null 2>&1 )
[ "$(cat B/a.txt 2>/dev/null)" = "v1" ] || {
    echo "FAIL: clone B did not check out c1 (a.txt=$(cat B/a.txt 2>/dev/null))" >&2
    exit 1
}

# A advances the branch tip.
sleep 0.02
echo v2 > a.txt
"$BE" put a.txt           > /dev/null 2>&1
"$BE" post 'c2'           > /dev/null 2>&1

# B is now lagging.  `be get` (default) fast-forwards + reports.
( cd B && "$BE" get --color > lag.color.out 2> lag.color.err )
banner_must "$ROOT/B/lag.color.out" "lagging/color"
row_must    "$ROOT/B/lag.color.out" "lagging/color" 'a\.txt'
[ "$(cat B/a.txt)" = "v2" ] || {
    echo "FAIL: lagging B did not fast-forward to c2 (a.txt=$(cat B/a.txt))" >&2
    exit 1
}

# A fresh second clone C lags by a NEW commit, fast-forwarded in --tlv:
# C clones at c2, then A advances to c3, then C's `be get --tlv` FFs and
# the H-hunk carries both the `?#<hashlet>` state uri and the file row.
cd "$ROOT"
mkdir C
( cd C && "$BE" get "file:$ROOT" > /dev/null 2>&1 )
sleep 0.02
echo v3 > a.txt
"$BE" put a.txt           > /dev/null 2>&1
"$BE" post 'c3'           > /dev/null 2>&1
( cd C && "$BE" get --tlv > lag.tlv.out 2> lag.tlv.err )
uri_must "$ROOT/C/lag.tlv.out" "lagging/tlv"
row_must "$ROOT/C/lag.tlv.out" "lagging/tlv" 'a\.txt'
[ "$(cat C/a.txt)" = "v3" ] || {
    echo "FAIL: lagging C did not fast-forward to c3 (a.txt=$(cat C/a.txt))" >&2
    exit 1
}

echo "get/52 OK: default GET state banner (BE-005/GET-026), color & tlv"
