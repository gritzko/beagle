#!/bin/sh
#  get/07-submodule-nosub — `be get --nosub` skips the submodule-mount
#  loop in sniff/GET.c.  Same parent/sub seed as case 05 (ssh://
#  parent with a `.gitmodules` + 160000 gitlink at vendor/sub), but
#  with `--nosub` we expect:
#    * parent files materialise (main.c)
#    * stderr carries the `submodule(s) skipped (--nosub)` line
#    * vendor/sub/ is NOT populated (no `lib.h`, no `.be` anchor)
#
#  Regression spec: a botched merge in sniff/GET.c parked stray git
#  conflict markers (`<<<<`, `||||`, `>>>>`) inside the if-conditions
#  that gate the --nosub vs. mount branches.  The file no longer
#  compiled, so every `be get` flavour broke.  Gating this case on
#  `--nosub` keeps the assertion-set close to what got broken.

. "$(dirname "$0")/../../lib/case.sh"

# Silence the "initial branch name" hint git 2.30+ prints on every
# `git init`; we pin `master` explicitly with `checkout -b master`.
export GIT_CONFIG_GLOBAL=/dev/null

[ -n "${HOME:-}" ] || { echo "get/07: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "get/07: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2
       echo "        keeper's ssh-side path resolution requires it" >&2
       exit 1;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

SUB_BARE="$SCRATCH/sub.git"
PARENT_BARE="$SCRATCH/parent.git"
SUB_SEED="$SCRATCH/sub-seed"
PARENT_SEED="$SCRATCH/parent-seed"
REL_SUB="$REL_SCRATCH/sub.git"
REL_PARENT="$REL_SCRATCH/parent.git"

cd "$SCRATCH"

# 1. seed sub.git with a single file (lib.h) on master ----------------
git init --bare "$SUB_BARE" >/dev/null
git init "$SUB_SEED" >/dev/null
git -C "$SUB_SEED" config user.email t@t
git -C "$SUB_SEED" config user.name  T
git -C "$SUB_SEED" checkout -b master >/dev/null 2>&1 || true
cp "$CASE/01.sub-lib.h" "$SUB_SEED/lib.h"
git -C "$SUB_SEED" add lib.h >/dev/null
git -C "$SUB_SEED" commit -qm 'sub: initial'
git -C "$SUB_SEED" push -q "$SUB_BARE" master:master

# 2. seed parent.git with main.c, .gitmodules, and a 160000 gitlink ---
SUB_TIP=$(git -C "$SUB_BARE" rev-parse master)
git init --bare "$PARENT_BARE" >/dev/null
git init "$PARENT_SEED" >/dev/null
git -C "$PARENT_SEED" config user.email t@t
git -C "$PARENT_SEED" config user.name  T
git -C "$PARENT_SEED" checkout -b master >/dev/null 2>&1 || true
cp "$CASE/02.parent-main.c" "$PARENT_SEED/main.c"
sed "s|@REL_SUB@|$REL_SUB|g" "$CASE/03.gitmodules.tmpl" \
    > "$PARENT_SEED/.gitmodules"
git -C "$PARENT_SEED" add main.c .gitmodules >/dev/null
git -C "$PARENT_SEED" update-index --add --cacheinfo 160000,"$SUB_TIP",vendor/sub
git -C "$PARENT_SEED" commit -qm 'parent: initial w/ submodule'
git -C "$PARENT_SEED" push -q "$PARENT_BARE" master:master

# 3. THE TEST: `be get --nosub ssh://.../parent.git?master` ----------
#    case.sh pre-creates $SCRATCH/.be so the test is shielded from
#    `be`'s upward walk for an ambient repo; we want a fresh wt root
#    *inside* SCRATCH, so drop the placeholder before cd-ing in.
rm -rf "$SCRATCH/.be"
mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get --nosub "ssh://localhost/$REL_PARENT?master" \
    > 01.get.got.out 2> 01.get.got.err
#  `be get` prints a status report on stdout (commit 366259dd);
#  assert the expected new-file lines instead of empty stdout.
for f in .gitmodules main.c; do
    grep -qE "^[[:space:]]*[0-9:]+[[:space:]]+new[[:space:]]+$f$" \
            01.get.got.out || {
        echo "FAIL: missing 'new $f' line in stdout" >&2
        cat 01.get.got.out >&2
        exit 1
    }
done

# 4. parent files materialised ---------------------------------------
[ -f main.c ] || { echo "FAIL: wt/main.c missing" >&2; exit 1; }
grep -q 'int main' main.c || {
    echo "FAIL: wt/main.c content unexpected:" >&2
    cat main.c >&2; exit 1
}

# 5. stderr carries the --nosub skip marker --------------------------
grep -q 'skipped (--nosub)' 01.get.got.err || {
    echo "FAIL: expected 'skipped (--nosub)' marker on stderr" >&2
    cat 01.get.got.err >&2; exit 1
}

# 6. sub mount-point dir exists, but its content is NOT materialised -
#    get_visit creates the dir for every WALK_KIND_SUB; the actual
#    mount (lib.h, .be anchor, .be/sub/ store) only happens inside
#    the GETCheckout submodule-mount loop, which --nosub skips.
[ -d vendor/sub ] || {
    echo "FAIL: vendor/sub/ mount-point dir missing" >&2; exit 1
}
[ ! -e vendor/sub/lib.h ] || {
    echo "FAIL: vendor/sub/lib.h should NOT exist under --nosub" >&2
    ls -la vendor/sub >&2; exit 1
}
[ ! -e vendor/sub/.be ] || {
    echo "FAIL: vendor/sub/.be anchor should NOT exist under --nosub" >&2
    ls -la vendor/sub >&2; exit 1
}
[ ! -d .be/sub ] || {
    echo "FAIL: .be/sub/ store should NOT exist under --nosub" >&2
    ls -la .be >&2; exit 1
}

# 7. parent wtlog has the `get` row ----------------------------------
[ -f .be/wtlog ] || { echo "FAIL: parent .be/wtlog missing" >&2; exit 1; }

# 8. Path B: manual `be get` for the sub --------------------------------
#    --nosub left vendor/sub/ as an empty mount-point dir.  The
#    canonical user-driven mount is then `cd vendor/sub && be get
#    ssh://…/sub.git`: cwd is a strict subdir of wt/.be/, so
#    be_sub_shard_setup fires, mkdirs wt/.be/sub/.be/ (seeding refs +
#    wtlog), writes vendor/sub/.be as a secondary-wt anchor pointing
#    at that fresh shard, then the normal fetch lands the sub's
#    keeper objects in the shard rather than the parent's trunk.
#    Plain ssh:// git URI — no `?branch` query (remote is a bare git
#    repo; wire protocol advertises refs).
mkdir -p vendor/sub
( cd vendor/sub && "$BE" get "ssh://localhost/$REL_SUB" \
      >../../02.manual.got.out 2>../../02.manual.got.err )
mrc=$?
[ "$mrc" = 0 ] || { echo "FAIL: manual be get exited $mrc; stderr:" >&2
                    cat 02.manual.got.err >&2; exit 1; }

#  Sub blob materialised by the manual fetch.
[ -f vendor/sub/lib.h ] || { echo "FAIL: manual: vendor/sub/lib.h missing" >&2
                              ls -la vendor/sub >&2; exit 1; }
grep -q 'SUB 1' vendor/sub/lib.h \
    || { echo "FAIL: manual: vendor/sub/lib.h content unexpected" >&2; exit 1; }

#  Secondary-wt anchor written into the mount.
[ -f vendor/sub/.be ] || { echo "FAIL: manual: vendor/sub/.be anchor missing" >&2; exit 1; }
[ ! -d vendor/sub/.be ] || { echo "FAIL: vendor/sub/.be must be a regular file" >&2; exit 1; }

#  Sub-shard populated under parent's .be/ — the whole point of Path B.
#  Flat layout per SUBS.plan.md: pack logs / refs / wtlog live directly
#  in `.be/<basename>/`, identical shape to the parent's trunk dir.
[ -d .be/sub ] || { echo "FAIL: manual: wt/.be/sub/ shard dir missing" >&2; exit 1; }
[ -f .be/sub/refs ]  || { echo "FAIL: manual: wt/.be/sub/refs missing" >&2
                          ls -la .be/sub >&2; exit 1; }
[ -f .be/sub/wtlog ] || { echo "FAIL: manual: wt/.be/sub/wtlog missing" >&2; exit 1; }
[ -s .be/sub/refs ]  || { echo "FAIL: manual: wt/.be/sub/refs empty (sub fetch did not record)" >&2
                          ls -la .be/sub >&2; exit 1; }
