#!/bin/sh
#  get/32-sub-same-project-twice — the SAME sub project mounted twice
#  by one parent, each gitlink pinned to a DIFFERENT branch/commit
#  (different hashes).  Both mounts share one store shard (`.be/sub/`,
#  keyed by url-basename), but each checks out its own pin.
#
#  Fixture (built inline; ssh to localhost like submodules.sh):
#      sub.git/     bare git upstream, TWO branches:
#                     master → A  (which.txt = "alpha")
#                     other  → B  (which.txt = "beta")   [child of A]
#      parent.git/  bare git upstream, master commit whose .gitmodules
#                   declares two sections, both url=$SUB_URL:
#                     first  pinned to A (master tip)
#                     second pinned to B (other  tip)
#
#  Asserts: both mounts materialise as secondary-wt anchors, each holds
#  its branch's bytes (first=alpha, second=beta — they differ), the
#  recorded pins differ (A vs B), and a single shared `.be/sub/` shard
#  carries both commits' objects.
#
#  Requires passwordless ssh to localhost (registered in
#  test/CMakeLists.txt's _BE_NEEDS_SSH_CASES).

. "$(dirname "$0")/../../lib/branches.sh"

export GIT_CONFIG_GLOBAL=/dev/null

#  keeper resolves ssh://localhost/<rel> as $HOME-relative on the peer
#  side, so $SCRATCH must live under $HOME.
[ -n "${HOME:-}" ] || fail "\$HOME unset"
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) fail "SCRATCH=$SCRATCH not under \$HOME=$HOME" ;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

#  Drop case.sh's scratch `.be/` so the inner wt is a clean primary.
rmdir "$SCRATCH/.be" 2>/dev/null || true

SUB_BARE="$SCRATCH/sub.git"
PARENT_BARE="$SCRATCH/parent.git"
SUB_URL="ssh://localhost/$REL_SCRATCH/sub.git"
PARENT_URL="ssh://localhost/$REL_SCRATCH/parent.git"

SUB_SEED="$ETMP/sub-seed"
PARENT_SEED="$ETMP/parent-seed"
rm -rf "$SUB_SEED" "$PARENT_SEED"

# --- sub upstream: two branches, two distinct tips --------------------
git init --bare "$SUB_BARE" >/dev/null
git init "$SUB_SEED" >/dev/null
git -C "$SUB_SEED" config user.email t@t
git -C "$SUB_SEED" config user.name  T
git -C "$SUB_SEED" checkout -b master >/dev/null 2>&1 || true

printf 'alpha\n' > "$SUB_SEED/which.txt"
git -C "$SUB_SEED" add which.txt >/dev/null
git -C "$SUB_SEED" commit -qm 'sub: branch A (alpha)'
SUB_A=$(git -C "$SUB_SEED" rev-parse HEAD)

git -C "$SUB_SEED" checkout -q -b other
printf 'beta\n' > "$SUB_SEED/which.txt"
git -C "$SUB_SEED" add which.txt >/dev/null
git -C "$SUB_SEED" commit -qm 'sub: branch B (beta)'
SUB_B=$(git -C "$SUB_SEED" rev-parse HEAD)

[ "$SUB_A" != "$SUB_B" ] || fail "fixture: sub branch tips collided"

git -C "$SUB_SEED" push -q "$SUB_BARE" master:master
git -C "$SUB_SEED" push -q "$SUB_BARE" other:other

# --- parent upstream: same sub url mounted twice, different pins ------
git init --bare "$PARENT_BARE" >/dev/null
git init "$PARENT_SEED" >/dev/null
git -C "$PARENT_SEED" config user.email t@t
git -C "$PARENT_SEED" config user.name  T
git -C "$PARENT_SEED" checkout -b master >/dev/null 2>&1 || true

printf 'int main(void){return 0;}\n' > "$PARENT_SEED/main.c"
{
    printf '[submodule "first"]\n\tpath = first\n\turl = %s\n'  "$SUB_URL"
    printf '[submodule "second"]\n\tpath = second\n\turl = %s\n' "$SUB_URL"
} > "$PARENT_SEED/.gitmodules"
git -C "$PARENT_SEED" add main.c .gitmodules >/dev/null
#  Two gitlinks, same url-basename, different pins.  Plumbing keeps the
#  seed offline (no eager `git submodule add` clone).
git -C "$PARENT_SEED" update-index --add --cacheinfo 160000,"$SUB_A",first
git -C "$PARENT_SEED" update-index --add --cacheinfo 160000,"$SUB_B",second
git -C "$PARENT_SEED" commit -qm 'parent: first@A second@B (same sub)'
git -C "$PARENT_SEED" push -q "$PARENT_BARE" master:master

note "fixture: sub A=$SUB_A B=$SUB_B; parent mounts first@A second@B"

# --- clone the parent recursively -------------------------------------
mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "be get exited $rc; stderr:
$(cat 01.get.got.err)"

# --- both mounts materialised as secondary-wt anchors -----------------
for m in first second; do
    [ -d "$m" ]        || fail "mount $m/ dir missing"
    [ -f "$m/.be" ]    || fail "$m/.be anchor missing (not a file)"
    [ ! -d "$m/.be" ]  || fail "$m/.be should be a regular file"
    [ -f "$m/which.txt" ] || fail "$m/which.txt missing"
done

# --- each mount carries its own branch's bytes ------------------------
[ "$(cat first/which.txt)"  = "alpha" ] \
    || fail "first/which.txt = '$(cat first/which.txt)' want 'alpha'"
[ "$(cat second/which.txt)" = "beta" ] \
    || fail "second/which.txt = '$(cat second/which.txt)' want 'beta'"
[ "$(cat first/which.txt)" != "$(cat second/which.txt)" ] \
    || fail "the two mounts hold identical bytes (pins not distinct)"

# --- recorded pins differ: first=A, second=B --------------------------
pin_of() {
    awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
               END { h=last; sub(/^[^#]*#/, "", h); print h }' "$1"
}
first_pin=$(pin_of first/.be)
second_pin=$(pin_of second/.be)
[ "$first_pin"  = "$SUB_A" ] \
    || fail "first pin mismatch: got '$first_pin' want '$SUB_A'"
[ "$second_pin" = "$SUB_B" ] \
    || fail "second pin mismatch: got '$second_pin' want '$SUB_B'"

# --- one shared shard (url-basename 'sub') holding both commits -------
[ -d .be/sub ]    || fail ".be/sub shard dir missing"
[ -s .be/sub/refs ] \
    || fail ".be/sub/refs missing or empty:
$(ls -la .be/sub 2>&1)"
#  The single shard must serve BOTH pins (one object pool, two
#  checkouts) — neither mount got a private shard.
[ ! -d .be/first ]  || fail "unexpected private shard .be/first"
[ ! -d .be/second ] || fail "unexpected private shard .be/second"

note "get/32-sub-same-project-twice: first@A + second@B share .be/sub/"
