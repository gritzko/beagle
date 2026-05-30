#!/bin/sh
#  post/20-push-prints-diff — `be post //origin` (FF push) prints the
#  pushed difference: the commits it introduced to the remote and the
#  files they touched, in ULOG status format on stdout.  Mirrors the
#  banner GET prints on checkout (VERBS.md §POST: POST(-push) shows the
#  remote-tip → pushed-tip range; PUT/force pushes stay silent).
#
#  Setup:
#    origin (bare) seeded with A on master; cloned into wt via ssh.
#    Two local commits on top of A:
#      R1  modify hello.c
#      R2  add extra.c
#    `be post //localhost` FF-pushes A->R2.  stdout must carry:
#      * two `<date>\tpost\t?<hashlet>#<subject>` rows (newest-first)
#      * `<date>\tadd\textra.c` and `<date>\tmod\thello.c`
#        (net A->R2 tree diff, path-sorted: extra.c before hello.c)
#
#  Requires passwordless ssh to localhost (gated under WITH_SSH).

. "$(dirname "$0")/../../lib/case.sh"

[ -n "${HOME:-}" ] || { echo "post/20: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "post/20: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2; exit 1;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

ORIGIN="$SCRATCH/origin.git"
SEED="$SCRATCH/seed"
REL_ORIGIN="$REL_SCRATCH/origin.git"

cd "$SCRATCH"

# 1. seed origin with A on master
git init --bare "$ORIGIN" >/dev/null
git init "$SEED" >/dev/null
git -C "$SEED" config user.email t@t
git -C "$SEED" config user.name  T
git -C "$SEED" checkout -b master >/dev/null || true
sleep 0.02; cp "$CASE/01.A.hello.c" "$SEED/hello.c"
git -C "$SEED" add . >/dev/null
git -C "$SEED" commit -qm A
git -C "$SEED" push -q "$ORIGIN" master:master

# 2. clone into wt via ssh (shield from $HOME repo per CLAUDE.md)
mkdir wt wt/.be && cd wt
"$BE" get "ssh://localhost/$REL_ORIGIN?master" \
    >01.clone.got.out 2>01.clone.got.err
match "$CASE/01.A.hello.c" hello.c

# 3. local commit R1 — modify hello.c
sleep 0.02; cp "$CASE/02.R1.hello.c" hello.c
"$BE" post 'R1 modify hello' >02.r1.got.out 2>02.r1.got.err

# 4. local commit R2 — add extra.c
sleep 0.02; cp "$CASE/03.extra.c" extra.c
"$BE" put extra.c        >03.put.got.out 2>03.put.got.err
"$BE" post 'R2 add extra' >04.r2.got.out 2>04.r2.got.err

# 5. FF-push to origin; stdout carries the pushed-difference banner.
"$BE" post "//localhost" >05.push.got.out 2>05.push.got.err || {
    echo "post/20: be post //localhost failed" >&2
    cat 05.push.got.err >&2
    exit 1; }

out=05.push.got.out
dump() { echo "--- $out was: ---" >&2; cat "$out" >&2; }

# 6. file rows: net A->R2 diff is `add extra.c` + `mod hello.c`.
grep -Eq 'add[[:space:]]+extra\.c$'  "$out" || { echo "post/20: no 'add extra.c' row" >&2; dump; exit 1; }
grep -Eq 'mod[[:space:]]+hello\.c$'  "$out" || { echo "post/20: no 'mod hello.c' row" >&2; dump; exit 1; }

# 7. exactly two commit rows (the two pushed commits, no extras), and
#    both subjects present (R1 newer than its parent → newest-first R2,R1).
n=$(grep -Ec 'post[[:space:]]+\?' "$out")
[ "$n" -eq 2 ] || { echo "post/20: expected 2 commit rows, got $n" >&2; dump; exit 1; }
grep -q 'R1' "$out" || { echo "post/20: R1 commit subject missing" >&2; dump; exit 1; }
grep -q 'R2' "$out" || { echo "post/20: R2 commit subject missing" >&2; dump; exit 1; }
