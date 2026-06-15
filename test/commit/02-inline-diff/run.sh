#!/bin/sh
#  commit/02-inline-diff — COMMIT-002.  `commit:?<ref>` reads like
#  `git show`: the commit metadata (headers + message) is followed
#  INLINE by the commit's full unified diff.  Per the dog boundary
#  (keeper owns `commit:`, graf owns `diff:`, separate binaries) keeper
#  does NOT re-roll diff rendering — the `be` dispatcher runs graf's
#  `diff:?<sha>` for the SAME resolved sha and RELAYS its hunk stream
#  after the commit-metadata hunk (the capture-and-replay the sub
#  recursion uses).  This supersedes COMMIT-001's navigable link.
#
#  Asserted in every render mode: Plain (default piped), Color
#  (--color), and TLV (--tlv).  The diff BODY (the changed token
#  `extra`) must appear in the same stream as the metadata header.

. "$(dirname "$0")/../../lib/branches.sh"

#  --- v1 ------------------------------------------------------------
printf 'int main(){return 0;}\n' >foo.c
"$BE" put foo.c >/dev/null
"$BE" post -m v1 '?v1' >/dev/null

#  --- v2 ------------------------------------------------------------
printf 'int main(){return 1;}\nint extra(){return 2;}\n' >foo.c
"$BE" put foo.c >/dev/null
"$BE" post -m v2 '?v2' >/dev/null

SHA=$("$BE" sha1:?v2 2>/dev/null)
[ -n "$SHA" ] || fail "sha1:?v2 resolved empty"

#  --- Plain: metadata header AND inline diff body in one stream ------
"$BE" commit:?v2 >01.plain.got.out 2>01.plain.got.err
rc=$?
[ "$rc" = 0 ] || fail "be commit:?v2 exited $rc; stderr:
$(cat 01.plain.got.err)"
grep -q "^commit $SHA" 01.plain.got.out || fail "commit: missing commit header; stdout:
$(cat 01.plain.got.out)"
grep -q "^author "     01.plain.got.out || fail "commit: missing author header"
grep -q "^v2"          01.plain.got.out || fail "commit: missing message body"
#  INLINE diff body: the changed token from the commit's own diff.
grep -q "extra"        01.plain.got.out || fail "commit: diff body NOT inlined (no 'extra'); stdout:
$(cat 01.plain.got.out)"
#  The unified-diff hunk header proves it's graf's diff, relayed.
grep -q "+int extra"   01.plain.got.out || fail "commit: inline diff lacks the added line; stdout:
$(cat 01.plain.got.out)"
empty 01.plain.got.err

#  --- Color: same content, ANSI-decorated (strip SGR before grep) ----
"$BE" --color commit:?v2 >02.color.got.out 2>02.color.got.err
rc=$?
[ "$rc" = 0 ] || fail "be --color commit:?v2 exited $rc; stderr:
$(cat 02.color.got.err)"
#  Strip CSI sequences so the assertions are colour-agnostic.
sed 's/\x1b\[[0-9;]*m//g' 02.color.got.out >02.color.plain.out
grep -q "commit $SHA" 02.color.plain.out || fail "commit --color: missing commit header; stdout:
$(cat 02.color.plain.out)"
grep -q "extra"       02.color.plain.out || fail "commit --color: diff body NOT inlined; stdout:
$(cat 02.color.plain.out)"

#  --- TLV: the wire stream carries both metadata and diff text -------
"$BE" --tlv commit:?v2 >03.tlv.got.out 2>03.tlv.got.err
rc=$?
[ "$rc" = 0 ] || fail "be --tlv commit:?v2 exited $rc; stderr:
$(cat 03.tlv.got.err)"
grep -aq "commit $SHA" 03.tlv.got.out || fail "commit --tlv: missing commit header in wire stream"
grep -aq "extra"       03.tlv.got.out || fail "commit --tlv: diff body NOT relayed into the wire stream; stdout (binary):
$(strings 03.tlv.got.out)"

note "commit/02: commit:?<ref> inlines the full diff (relayed from graf) after the metadata, in Plain/Color/TLV"
