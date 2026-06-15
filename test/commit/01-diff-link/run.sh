#!/bin/sh
#  commit/01-diff-link — COMMIT-002 supersedes COMMIT-001.  `commit:?<ref>`
#  no longer emits a navigable `diff:?<sha>` LINK after the metadata; it
#  now inlines the FULL diff (relayed from graf — see commit/02 for the
#  Plain/Color/TLV + sub coverage).  This case keeps the two assertions
#  that survive the change: (1) commit metadata is intact, and (2) the
#  blame-link form `commit:?<8hex>#<message>` resolves to the same commit
#  (the hex query is authoritative; the fragment is a human label, never
#  an object id).

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

#  --- metadata present, full diff inlined (no separate link) --------
"$BE" commit:?v2 >01.commit.got.out 2>01.commit.got.err
rc=$?
[ "$rc" = 0 ] || fail "be commit:?v2 exited $rc; stderr:
$(cat 01.commit.got.err)"
grep -q "^commit $SHA"  01.commit.got.out || fail "commit: missing commit header; stdout:
$(cat 01.commit.got.out)"
grep -q "^author "      01.commit.got.out || fail "commit: missing author header"
grep -q "^v2"           01.commit.got.out || fail "commit: missing message body"
#  The diff is inlined (COMMIT-002): the changed token rides in the same
#  stream as the metadata — no navigable link, no separate fetch.
grep -q "extra"         01.commit.got.out || fail "commit: diff not inlined (no 'extra'); stdout:
$(cat 01.commit.got.out)"
empty 01.commit.got.err

#  --- BLAME link form: commit:?<8hex>#<message> resolves -------------
#  Blame run-hunks address a commit as `commit:?<8-char-hashlet>#<subject>`
#  — an 8-char hashlet QUERY plus the message subject as a human LABEL in
#  the fragment.  The hex query is the authoritative address; the
#  fragment must NOT be parsed as an object id.  So this MUST resolve to
#  the very same commit as the bare hashlet, the message (and its spaces)
#  notwithstanding.
H8=$(printf '%.8s' "$SHA")
"$BE" "commit:?$H8#CSS-like AST selectors" >04.blamelink.got.out 2>04.blamelink.got.err
rc=$?
[ "$rc" = 0 ] || fail "be commit:?$H8#<msg> (blame link) exited $rc; stderr:
$(cat 04.blamelink.got.err)"
grep -q "^commit $SHA" 04.blamelink.got.out || fail "commit: blame-link hashlet+message did not resolve to $SHA; stdout:
$(cat 04.blamelink.got.out)"
empty 04.blamelink.got.err

note "commit/01: commit:?<ref> keeps metadata + inlines the diff (COMMIT-002);" \
     "commit:?<8hex>#<message> blame-link resolves to the same commit"
