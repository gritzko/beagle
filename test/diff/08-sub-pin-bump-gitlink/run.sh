#!/bin/sh
#  diff/08-sub-pin-bump-gitlink — DIFF-001.  `be diff:?<sha>` on a commit
#  whose only change is a submodule PIN BUMP must render the gitlink
#  change `<sub> <old-pin>..<new-pin>`.  Pre-fix it showed NOTHING: the
#  ref-vs-ref tree-diff dropped gitlink (mode 160000) entries at
#  collection, so a pure pin-bump commit diffed to empty.

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

#  sub `ch` at c1, parent `par` pinning chsub→c1.
mkg ch  || { echo "FAIL(setup): git init ch"; exit 1; }
printf 'v1\n' > ch/c.txt; git -C ch add -A; git -C ch commit -qm c1
mkg par || { echo "FAIL(setup): git init par"; exit 1; }
printf 'par\n' > par/p.txt
git -C par -c protocol.file.allow=always submodule add -q "$SCRATCH/ch" chsub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add"; exit 1; }
git -C par add -A; git -C par commit -qm p1

#  advance the sub to c2, bump the pin in a NEW parent commit.
printf 'v2\n' > ch/c.txt; git -C ch add -A; git -C ch commit -qm c2
C2=$(git -C ch rev-parse HEAD)
git -C par/chsub fetch -q origin >/dev/null 2>&1
git -C par/chsub checkout -q "$C2"
git -C par add chsub; git -C par commit -qm 'bump chsub'

#  clone into a beagle store; the tip is the pin-bump commit.
mkdir -p B1/.be
( cd B1 && "$BE" get "file:$SCRATCH/par" >"$SCRATCH/01.out" 2>"$SCRATCH/01.err" ) \
    || { cat "$SCRATCH/01.err" >&2; echo "FAIL(setup): clone par into B1" >&2; exit 1; }
TIP=$( cd B1 && "$BE" sha1:'?master' )
[ -n "$TIP" ] || { echo "FAIL: B1 has no tip" >&2; exit 1; }

#  --- DIFF-001: the pin-bump commit must render a chsub gitlink line ---
( cd B1 && "$BE" diff:"?$TIP" >"$SCRATCH/02.out" 2>"$SCRATCH/02.err" )
grep -Eq 'chsub +[0-9a-f]{40}\.\.[0-9a-f]{40}' "$SCRATCH/02.out" || {
    echo "DIFF-001: pin-bump commit did not render a chsub gitlink line" >&2
    echo "--- stdout ---"; cat "$SCRATCH/02.out" >&2
    echo "--- stderr ---"; cat "$SCRATCH/02.err" >&2
    exit 1; }

#  --- DIFF-001 part-b: the gitlink line must be FOLLOWED by the sub's
#  actual content diff for the pin range (c.txt: v1 -> v2), path-prefixed
#  under the mount.  Pre-fix the projector fan-out replayed the parent URI
#  verbatim (`diff:?<parent-sha>`) into the sub — no such commit there →
#  KEEPNONE — so the sub content vanished.  Option B rewrites each sub's
#  child URI to `diff:?<old-pin>#<new-pin>` from graf's gitlink pin pair.
grep -q '+v2' "$SCRATCH/02.out" || {
    echo "DIFF-001 part-b: pin-bump did not render the sub content diff" >&2
    echo "  expected the chsub content change (c.txt: v1 -> v2)" >&2
    echo "--- stdout ---"; cat "$SCRATCH/02.out" >&2
    echo "--- stderr ---"; cat "$SCRATCH/02.err" >&2
    exit 1; }
grep -q 'chsub/c.txt' "$SCRATCH/02.out" || {
    echo "DIFF-001 part-b: sub content diff not path-prefixed under chsub/" >&2
    echo "--- stdout ---"; cat "$SCRATCH/02.out" >&2
    exit 1; }

#  --- DIFF-001 part-b: --nosub keeps the gitlink line but drops the sub
#  content diff (the documented opt-out).
( cd B1 && "$BE" diff:"?$TIP" --nosub >"$SCRATCH/03.out" 2>"$SCRATCH/03.err" )
grep -Eq 'chsub +[0-9a-f]{40}\.\.[0-9a-f]{40}' "$SCRATCH/03.out" || {
    echo "DIFF-001 part-b: --nosub dropped the gitlink line too" >&2
    echo "--- stdout ---"; cat "$SCRATCH/03.out" >&2
    exit 1; }
if grep -q '+v2' "$SCRATCH/03.out"; then
    echo "DIFF-001 part-b: --nosub still rendered the sub content diff" >&2
    echo "--- stdout ---"; cat "$SCRATCH/03.out" >&2
    exit 1
fi

#  --- DIFF-002: the pin-bump diff must travel THROUGH the pager (bro),
#  not bypass it.  Pre-fix the gitlink line `<sub> <old>..<new>` was
#  written as a RAW `fprintf(stdout)` text line that, in `--tlv` mode,
#  led the stream UN-ENVELOPED before the hunk `H` records.  A TLV
#  consumer (`bro_drain_tlv` → `HUNKu8sDrain`) mis-parses those leading
#  bytes, fails the `HUNK_TLV` type gate, and STOPS at offset 0 — so the
#  whole stream (gitlink line + sub content) folded to ZERO renderable
#  hunks ("be: no results").  This is THE channel bug: a pin-bump commit
#  whose only parent-level change is the gitlink line rendered EMPTY in
#  the pager though the diff itself is correct.
#
#  Two deterministic, no-PTY signals (mirrors diff/09):
#    (1) `be --color diff:?<sha>` forces the bro-pager branch; with bro's
#        stdout a plain file it does its one-shot non-interactive drain
#        (BROPipeRun → bro_drain_tlv → BROPlain).  The dirtied sub content
#        (`v2`) must appear in bro's OWN output, proving the stream
#        survived the TLV drain.
#    (2) the sub hunk header must be the BRO-002 banner band
#        (`38;5;0;48;5;230m … chsub/c.txt`), proving it was bro-rendered
#        (passed through the pager), not raw relay ANSI written past bro.
( cd B1 && "$BE" --color diff:"?$TIP" >"$SCRATCH/04.out" 2>"$SCRATCH/04.err" )
rc=$?
[ "$rc" = 0 ] || { echo "DIFF-002: be --color diff:?$TIP exited $rc" >&2
    cat "$SCRATCH/04.err" >&2; exit 1; }
grep -aq 'v2' "$SCRATCH/04.out" || {
    echo "DIFF-002: pin-bump sub content missing from pager stream" >&2
    echo "  (raw gitlink fprintf broke bro_drain_tlv at offset 0 → 0 hunks)" >&2
    cat -v "$SCRATCH/04.out" | head -40 >&2; exit 1; }
grep -aq -- '48;5;230m.*chsub/c.txt' "$SCRATCH/04.out" || {
    echo "DIFF-002: pin-bump sub hunk not bro-rendered (bypassed the pager)" >&2
    echo "  expected a THEME_BANNER-wrapped 'chsub/c.txt' header" >&2
    cat -v "$SCRATCH/04.out" | head -40 >&2; exit 1; }

echo "diff/08-sub-pin-bump-gitlink: OK"
