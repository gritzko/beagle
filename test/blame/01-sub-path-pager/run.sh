#!/bin/sh
#  blame/01-sub-path-pager — BE-004.  A path-scoped projector whose path
#  lands inside a MOUNTED SUB (`blame:chsub/c.txt`) must make the SAME
#  pager-vs-direct decision as the same projector on a PARENT file
#  (`blame:p.txt`): a projection is a projection regardless of which
#  shard owns the path.
#
#  Pre-fix (BE-004): `BEProjector` ran `BEProjectorRouteToMount` and
#  `return`ed BEFORE the bro-pager block, so a sub-path projection
#  re-emitted the captured sub TLV through the parent's `HUNKMode`
#  (`HUNKOutColor` under --color) and `HUNKu8sRelay` painted ANSI to
#  bare stdout — NO pager.  A parent-file projection funnelled through
#  bro.  Same verb, same mode, opposite UX.
#
#  Hermetic handle on the otherwise tty-only pager decision: `--color`
#  forces `HUNKMode=HUNKOutColor` even on a pipe (dog/CLI.c), so the
#  pager IS spawned; bro with a non-tty stdout does a one-shot ANSI
#  *dump* (bro/BRO.c BROPipeRun: `if (!isatty(STDOUT_FILENO)) dump`),
#  no interactive takeover, no hang.  bro's dump column-PADS every line
#  to the terminal width with a background SGR fill, so a bro-paged
#  header line is FAR wider than the raw `HUNKu8sRelay` header.  The
#  test asserts the sub-path color header is paged exactly like the
#  parent-file one (≈ same wide width), NOT the short raw-relay form.
#
#  --plain / --tlv must stay unchanged (no pager fold): the raw relay
#  re-emits in the resolved mode exactly as before.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null
rm -rf "$SCRATCH/.be"
command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }

mkg() {
    git init -q -b master "$1" >/dev/null 2>&1 || return 1
    git -C "$1" config user.email t@t
    git -C "$1" config user.name  T
    git -C "$1" config protocol.file.allow always
}

#  sub `ch` (a c.txt), parent `par` pinning ch→chsub plus its own p.txt.
mkg ch  || { echo "FAIL(setup): git init ch"; exit 1; }
printf 'one\ntwo\nthree\n' > ch/c.txt
git -C ch add -A; git -C ch commit -qm c1
mkg par || { echo "FAIL(setup): git init par"; exit 1; }
printf 'par body\n' > par/p.txt
git -C par -c protocol.file.allow=always submodule add -q "$SCRATCH/ch" chsub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add"; exit 1; }
git -C par add -A; git -C par commit -qm p1

#  clone into a beagle store; chsub mounts as a worktree dir.
mkdir -p B1/.be
( cd B1 && "$BE" get "file:$SCRATCH/par" >"$SCRATCH/00.out" 2>"$SCRATCH/00.err" ) \
    || { cat "$SCRATCH/00.err" >&2; echo "FAIL(setup): clone par into B1" >&2; exit 1; }
[ -f B1/chsub/c.txt ] || { echo "FAIL(setup): chsub not mounted" >&2; ls -la B1 >&2; exit 1; }

#  header_width FILE -> byte length of the first output line (the blame
#  banner).  bro's one-shot dump pads it to the terminal width; the raw
#  relay does not.
header_width() { head -1 "$1" | wc -c | tr -d ' '; }

#  --- parent-file color blame: the reference pager route -------------
( cd B1 && "$BE" --color blame:p.txt >"$SCRATCH/par.color" 2>"$SCRATCH/par.err" )
[ -s "$SCRATCH/par.color" ] || { echo "FAIL: parent blame empty" >&2; cat "$SCRATCH/par.err" >&2; exit 1; }
PARW=$(header_width "$SCRATCH/par.color")

#  --- sub-path color blame: must take the SAME route ----------------
( cd B1 && "$BE" --color blame:chsub/c.txt >"$SCRATCH/sub.color" 2>"$SCRATCH/sub.err" )
[ -s "$SCRATCH/sub.color" ] || { echo "FAIL: sub blame empty" >&2; cat "$SCRATCH/sub.err" >&2; exit 1; }
SUBW=$(header_width "$SCRATCH/sub.color")

#  --- plain sub-path blame: the raw relay (no bro fold) reference ----
( cd B1 && "$BE" --plain blame:chsub/c.txt >"$SCRATCH/sub.plain" 2>/dev/null )
[ -s "$SCRATCH/sub.plain" ] || { echo "FAIL: plain sub blame empty" >&2; exit 1; }
PLAINW=$(header_width "$SCRATCH/sub.plain")

#  BE-004 core: the sub-path color header is bro-paged just like the
#  parent file — same wide width (allow a small per-line slack since the
#  banner *text* differs between the two files).
DELTA=$(( PARW > SUBW ? PARW - SUBW : SUBW - PARW ))
[ "$DELTA" -le 8 ] || {
    echo "BE-004: sub-path color blame did NOT take the parent's pager route" >&2
    echo "  parent header width=$PARW  sub header width=$SUBW (delta=$DELTA)" >&2
    echo "--- parent ---"; head -1 "$SCRATCH/par.color" | cat -v >&2
    echo "--- sub ---";    head -1 "$SCRATCH/sub.color" | cat -v >&2
    exit 1; }

#  Regression guard: the buggy raw-relay form emits a SHORT (unpadded)
#  header.  The bro-paged sub header must be strictly wider than the
#  unpadded plain header — i.e. it really went through the pager, it is
#  not just the old raw ANSI relay wearing colour.
[ "$SUBW" -gt "$PLAINW" ] || {
    echo "BE-004: sub-path color header is not pager-padded (raw relay regression)" >&2
    echo "  sub color width=$SUBW  plain width=$PLAINW (color must be wider)" >&2
    exit 1; }

#  --- --plain / --tlv unchanged: no pager fold ----------------------
#  Plain stays the raw relay (no SGR escapes at all).
LC_ALL=C grep -q "$(printf '\033')" "$SCRATCH/sub.plain" && {
    echo "BE-004: --plain sub blame leaked ANSI (should be raw plain relay)" >&2
    cat -v "$SCRATCH/sub.plain" >&2; exit 1; }

#  --tlv stays a capturable HUNK TLV stream (machine-parseable), never
#  paged.  The relayed sub hunk carries the path-prefixed blame URI.
( cd B1 && "$BE" --tlv blame:chsub/c.txt >"$SCRATCH/sub.tlv" 2>/dev/null )
[ -s "$SCRATCH/sub.tlv" ] || { echo "FAIL: --tlv sub blame empty" >&2; exit 1; }
strings "$SCRATCH/sub.tlv" | grep -q 'chsub' || {
    echo "BE-004: --tlv sub blame lost the chsub path prefix" >&2
    strings "$SCRATCH/sub.tlv" >&2; exit 1; }

echo "PASS blame/01-sub-path-pager (parent=$PARW sub=$SUBW plain=$PLAINW)"
