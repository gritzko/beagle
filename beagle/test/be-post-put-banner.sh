#!/bin/sh
#  be-post-put-banner.sh — POST-018 / BE-005.
#
#  `be put` (staging) and `be post` (a real commit) report through the
#  shared ROWS state banner — a pale-yellow `put:` / `post:` hunk header
#  — NOT the old raw `sniff: staged N put row(s)` / `sniff: commit <hex>`
#  stderr lines.  Checked over (verb, mode): `--color` (pale-yellow band
#  + rows) and `--tlv` (H-hunk banner uri + rows).  Every capture asserts
#  the report rides STDOUT and stderr carries NO raw report line.

. "$(dirname "$0")/verbcheck.sh"
. "$(dirname "$0")/setup-primitives.sh"

BAND='48;5;230m'   # THEME_BANNER pale-yellow bg (the ROWS state banner)

band_must() {  # FILE LABEL PAT — a pale-yellow banner line matching PAT
    grep -aq -- "${BAND}.*$3" "$1" && return 0
    echo "FAIL ($TEST_ID/$2): no pale-yellow '$3' state banner in:" >&2
    cat -v "$1" >&2; exit 1
}
row_must() {   # FILE LABEL PAT — a row matching PAT present (-a binary-safe)
    grep -aqE -- "$3" "$1" && return 0
    echo "FAIL ($TEST_ID/$2): expected a '$3' row under the banner:" >&2
    cat -v "$1" >&2; exit 1
}
no_raw() {     # FILE LABEL — stderr must NOT carry a raw report line
    grep -aqE 'sniff: (commit|staged) ' "$1" || return 0
    echo "FAIL ($TEST_ID/$2): raw report line leaked to stderr:" >&2
    cat -v "$1" >&2; exit 1
}

vc_step "setup: fresh wt + trunk"
vc_fresh_wt
sp_seed_trunk

vc_step "be put a.txt --color: 'put:' banner + staged rows on stdout"
echo "a v1" > a.txt
"$BE" put a.txt --color > "$TMP/put.color.out" 2> "$TMP/put.color.err"
band_must "$TMP/put.color.out" put/color 'put put:'
row_must  "$TMP/put.color.out" put/color 'a[.]txt'
no_raw    "$TMP/put.color.err" put/color

vc_step "be post --color: 'post:' banner + commit row on stdout"
"$BE" post -m 'banner check post' --color > "$TMP/post.color.out" 2> "$TMP/post.color.err"
band_must "$TMP/post.color.out" post/color 'post post:'
row_must  "$TMP/post.color.out" post/color 'banner check post'
no_raw    "$TMP/post.color.err" post/color

vc_step "be put b.txt --tlv: H-hunk 'put:' uri + row"
echo "b v1" > b.txt
"$BE" put b.txt --tlv > "$TMP/put.tlv.out" 2> "$TMP/put.tlv.err"
row_must "$TMP/put.tlv.out" put/tlv 'put:'
row_must "$TMP/put.tlv.out" put/tlv 'b[.]txt'
no_raw   "$TMP/put.tlv.err" put/tlv

vc_step "be post --tlv: H-hunk 'post:' uri + commit row"
"$BE" post -m 'second banner commit' --tlv > "$TMP/post.tlv.out" 2> "$TMP/post.tlv.err"
row_must "$TMP/post.tlv.out" post/tlv 'post:'
row_must "$TMP/post.tlv.out" post/tlv 'second banner commit'
no_raw   "$TMP/post.tlv.err" post/tlv

vc_note "put/post report via the shared ROWS state banner; stderr clean of raw report lines"
echo "=== be-post-put-banner: OK ==="
