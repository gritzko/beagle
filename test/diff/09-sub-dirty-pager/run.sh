#!/bin/sh
#  diff/09-sub-dirty-pager — DIFF-002.  `be diff` (working-tree) on a
#  parent whose ONLY change lives inside a mounted submodule's worktree
#  must route the sub's content diff into the SAME pager stream as the
#  parent — never dump it to the bare terminal after the pager (bro)
#  quits.  Pre-fix: on the pager (bro) path the parent diff (empty) went
#  through bro, which showed "nothing!", while the sub diff was relayed
#  straight to the inherited stdout — bypassing bro entirely — and so
#  surfaced only AFTER bro exited.
#
#  We exercise the SAME code path bro takes interactively, but
#  deterministically (no PTY / no `q` keystroke): `be --color` forces the
#  bro-pager branch (HUNKMode == Color), and with bro's stdout being a
#  plain file bro takes its one-shot non-interactive drain
#  (BROPipeRun → BROPlain).  The discriminator is the SUB hunk header's
#  RENDER FORMAT:
#    - fix:  the sub TLV is fed into bro's stdin, so bro renders the
#            header as `--- <sub>/<file>:<line> ---` (it passed through
#            the pager).
#    - bug:  the sub is relay-rendered ANSI written PAST bro, so its
#            header is `diff:<sub>/<file>#L<line>` (HUNKu8sFeedColor) and
#            bro's own output is just the empty-parent "nothing".
#  So a `--- <sub>/...` header proves the sub diff travelled through the
#  pager; a `diff:<sub>/...#L` header proves it bypassed it.

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

#  sub `ch` with a content file; parent `par` mounting it at chsub.
mkg ch  || { echo "FAIL(setup): git init ch"; exit 1; }
printf 'alpha\nbeta\ngamma\n' > ch/c.txt; git -C ch add -A; git -C ch commit -qm c1
mkg par || { echo "FAIL(setup): git init par"; exit 1; }
printf 'par\n' > par/p.txt
git -C par -c protocol.file.allow=always submodule add -q "$SCRATCH/ch" chsub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add"; exit 1; }
git -C par add -A; git -C par commit -qm p1

#  clone into a beagle store with the sub mounted.
mkdir -p B1/.be
( cd B1 && "$BE" get "file:$SCRATCH/par" >"$SCRATCH/01.out" 2>"$SCRATCH/01.err" ) \
    || { cat "$SCRATCH/01.err" >&2; echo "FAIL(setup): clone par into B1" >&2; exit 1; }
[ -f B1/p.txt ]       || { echo "FAIL(setup): parent p.txt missing" >&2; exit 1; }
[ -f B1/chsub/c.txt ] || { echo "FAIL(setup): sub chsub/c.txt missing" >&2; exit 1; }

#  Dirty ONLY the submodule's worktree — the parent stays clean.
printf 'DIRTYSUBLINE\n' >> B1/chsub/c.txt

#  --- (a) plain non-tty: combined stream already correct -------------
( cd B1 && "$BE" diff >"$SCRATCH/02.out" 2>"$SCRATCH/02.err" )
grep -q 'DIRTYSUBLINE' "$SCRATCH/02.out" || {
    echo "DIFF-002(non-tty): sub content missing from combined stdout" >&2
    echo "--- stdout ---"; cat "$SCRATCH/02.out" >&2
    echo "--- stderr ---"; cat "$SCRATCH/02.err" >&2
    exit 1; }

#  --- (b) pager path: the sub diff must travel THROUGH bro -----------
#  `--color` forces the bro branch; bro's stdout is this file, so bro
#  drains TLV one-shot and exits (no interactive hang).
( cd B1 && "$BE" --color diff: >"$SCRATCH/03.out" 2>"$SCRATCH/03.err" )
rc=$?
[ "$rc" = 0 ] || { echo "DIFF-002(pager): be --color diff: exited $rc" >&2
    cat "$SCRATCH/03.err" >&2; exit 1; }

#  The sub hunk must be PRESENT and rendered by BRO (`--- chsub/...`),
#  proving it passed through the pager.
grep -aq 'DIRTYSUBLINE' "$SCRATCH/03.out" || {
    echo "DIFF-002(pager): sub content missing from pager stream" >&2
    cat -v "$SCRATCH/03.out" | head -40 >&2; exit 1; }
grep -aq -- '--- chsub/c.txt' "$SCRATCH/03.out" || {
    echo "DIFF-002(pager): sub hunk not bro-rendered (bypassed the pager)" >&2
    echo "  expected a '--- chsub/c.txt:<line> ---' header" >&2
    cat -v "$SCRATCH/03.out" | head -40 >&2; exit 1; }

#  The bug signature — a relay-ANSI `diff:chsub/...#L` header that
#  bypassed bro — must NOT appear for the sub.
if grep -aq 'diff:chsub/c.txt#L' "$SCRATCH/03.out"; then
    echo "DIFF-002(pager): sub hunk relay-rendered past bro (the bug)" >&2
    cat -v "$SCRATCH/03.out" | head -40 >&2; exit 1
fi

echo "diff/09-sub-dirty-pager: OK"
