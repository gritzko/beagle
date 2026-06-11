#!/bin/sh
#  47-path-scope-no-sub-recurse — SUBS-018.  A path-scoped `be get <file>`
#  whose path is NOT under any submodule mount must NOT recurse into (and
#  re-checkout) the submodule.  Pre-fix, `be get --force p.txt` ran the
#  sub pass unscoped and re-materialised the whole `chsub` mount
#  (`be: get chsub` + every file rewritten), flooding output for a
#  one-file get even though `p.txt` is not under `chsub`.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null
rm -rf "$SCRATCH/.be"
command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }
mkg() {  # $1 = repo dir — quiet init with file:// submodules allowed
    git init -q -b master "$1" >/dev/null 2>&1 || return 1
    git -C "$1" config user.email t@t
    git -C "$1" config user.name  T
    git -C "$1" config protocol.file.allow always
}
mkg ch  || { echo "FAIL(setup): git init ch";  exit 1; }
printf 'ch\n' > ch/c.txt; git -C ch add -A; git -C ch commit -qm ch
mkg par || { echo "FAIL(setup): git init par"; exit 1; }
printf 'par\n' > par/p.txt
git -C par -c protocol.file.allow=always submodule add -q "$SCRATCH/ch" chsub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add ch → par"; exit 1; }
git -C par add -A; git -C par commit -qm par
mkdir -p B1/.be
( cd B1 && "$BE" get "file:$SCRATCH/par" >"$SCRATCH/01.out" 2>"$SCRATCH/01.err" ) \
    || { cat "$SCRATCH/01.err" >&2; echo "FAIL(setup): clone git par into B1" >&2; exit 1; }
[ -f B1/chsub/c.txt ] || { echo "FAIL(setup): sub chsub not mounted" >&2; exit 1; }

#  --- SUBS-018: path-scoped get of a NON-sub file → no sub recursion --
#  `p.txt` lives at the parent root, not under `chsub`, so the sub pass
#  must touch no submodule.
( cd B1 && "$BE" get --force p.txt >"$SCRATCH/02.out" 2>"$SCRATCH/02.err" ) \
    || { cat "$SCRATCH/02.err" >&2; echo "FAIL: be get --force p.txt failed" >&2; exit 1; }
grep -q 'be: get chsub' "$SCRATCH/02.err" && {
    echo "SUBS-018: path-scoped get of p.txt wrongly recursed into chsub" >&2
    cat "$SCRATCH/02.err" >&2; exit 1; }
[ -f B1/chsub/c.txt ] || { echo "FAIL: chsub vanished after scoped get" >&2; exit 1; }
echo "get/47-path-scope-no-sub-recurse: OK"
