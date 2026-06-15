#!/bin/sh
#  commit/01-diff-link — COMMIT-001.  `commit:?<ref>` surfaces the
#  commit's diff after its metadata.  Per the dog boundary (keeper owns
#  `commit:`, graf owns `diff:`) it does NOT inline the diff — it emits a
#  navigable U-token link to graf's `diff:?<sha>` for the SAME sha the
#  URI resolved to.  Bro follows that link (BRO.c U-token click) and
#  graf renders the commit-vs-first-parent diff (GRAF.exe.c `diff:?<sha>`
#  commit-show form, which is why the link is the QUERY form, not `#`).

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

#  --- metadata still present (CLI mode elides U-token link bytes) ---
"$BE" commit:?v2 >01.commit.got.out 2>01.commit.got.err
rc=$?
[ "$rc" = 0 ] || fail "be commit:?v2 exited $rc; stderr:
$(cat 01.commit.got.err)"
grep -q "^commit $SHA"  01.commit.got.out || fail "commit: missing commit header; stdout:
$(cat 01.commit.got.out)"
grep -q "^author "      01.commit.got.out || fail "commit: missing author header"
grep -q "^v2"           01.commit.got.out || fail "commit: missing message body"
empty 01.commit.got.err

#  --- diff link present in the hunk stream (TLV exposes U-bytes) ----
#  The link must target graf's `diff:?<sha>` for the SAME resolved sha.
"$BE" --tlv commit:?v2 >02.tlv.got.out 2>02.tlv.got.err
rc=$?
[ "$rc" = 0 ] || fail "be --tlv commit:?v2 exited $rc; stderr:
$(cat 02.tlv.got.err)"
grep -aq "diff:?$SHA" 02.tlv.got.out || fail "commit: no diff:?<sha> link to the resolved sha; link URIs found:
$(grep -ao 'diff:?[0-9a-f]*\|tree:#[0-9a-f]*\|commit:#[0-9a-f]*' 02.tlv.got.out)"

#  --- the link target actually resolves to a non-empty diff ---------
"$BE" "diff:?$SHA" >03.diff.got.out 2>03.diff.got.err
rc=$?
[ "$rc" = 0 ] || fail "be diff:?$SHA (link target) exited $rc; stderr:
$(cat 03.diff.got.err)"
grep -q "extra" 03.diff.got.out || fail "diff: link target did not render the commit's change; stdout:
$(cat 03.diff.got.out)"

note "commit/01: commit:?<ref> keeps metadata and links diff:?<sha> (same sha), which renders"
