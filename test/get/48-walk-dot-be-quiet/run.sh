#!/bin/sh
#  48-walk-dot-be-quiet — POST-015 (spam half, independent of DIS-038).
#  The keeper tree walk must skip a `.be` entry SILENTLY instead of
#  printing `walk: bad path '.be', skip` once per scanned tree.  A tree
#  carrying a `.be` (the store anchor name) otherwise floods stderr with
#  hundreds of identical lines that bury the real output.  A genuinely
#  invalid path still warns (that line is a real diagnostic).

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null
rm -rf "$SCRATCH/.be"
command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }

#  A git repo with a committed `.be` entry (force-added past ignores).
git init -q -b master g
git -C g config user.email t@t; git -C g config user.name T
printf 'real\n' > g/real.txt
printf 'junk\n' > g/.be
git -C g add -f real.txt .be >/dev/null 2>&1
git -C g commit -qm 'tree with .be'

#  Clone into beagle: the tree walk encounters `.be` and must stay quiet.
mkdir -p B/.be
( cd B && "$BE" get "file:$SCRATCH/g" >"$SCRATCH/g.out" 2>"$SCRATCH/g.err" )
[ -f B/real.txt ] || { echo "FAIL: clone did not check out real.txt" >&2
                       cat "$SCRATCH/g.err" >&2; exit 1; }
n=$(grep -c "bad path '.be'" "$SCRATCH/g.err" || true)
[ "$n" -eq 0 ] || {
    echo "POST-015: walk spammed 'bad path .be' $n time(s) — must be silent" >&2
    grep "bad path" "$SCRATCH/g.err" | head >&2; exit 1; }
echo "get/48-walk-dot-be-quiet: OK"
