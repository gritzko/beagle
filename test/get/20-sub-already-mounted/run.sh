#!/bin/sh
#  get/20-sub-already-mounted — second `be get` on a parent whose
#  submodule is already mounted at the requested pin MUST skip the
#  upstream fetch.  Regression for "be get hangs on submod update":
#  SNIFFSubMount used to call WIREFetchAll unconditionally on every
#  parent get, burning a network round-trip per sub even when the
#  wt was already at pin.
#
#  Setup (submodules.sh):
#      parent.git/  pins vendor/sub at $PARENT_PINNED (= $SUB_C2)
#      sub.git/     bare sub upstream
#
#  Test:
#      1. First `be get $PARENT_URL?master` — real fetch + mount.
#      2. Move $SUB_BARE aside so any subsequent sub fetch fails fast.
#      3. Second `be get $PARENT_URL?master` — must succeed AND
#         leave vendor/sub intact.
#      4. Assert SNIFFSubMount's debug trace shows the "already at
#         pin — skip fetch" line for vendor/sub.

. "$(dirname "$0")/../../lib/submodules.sh"

mkdir wt && cd wt

# --- first checkout: real fetch + mount ------------------------------
"$BE" get "$PARENT_URL?master" >01.get.got.out 2>01.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "first be get exited $rc; stderr:
$(cat 01.get.got.err)"
[ -f vendor/sub/core.c ] || fail "vendor/sub not mounted on first get"
grep -q 'sub_inc' vendor/sub/core.c \
    || fail "vendor/sub/core.c not at SUB_C2 after first get"

# --- break the sub upstream: any later fetch would fail --------------
mv "$SUB_BARE" "$SUB_BARE.gone"

# --- second checkout: sub already at pin → must skip fetch ----------
"$BE" get "$PARENT_URL?master" >02.get.got.out 2>02.get.got.err
rc=$?
[ "$rc" = 0 ] || fail "second be get exited $rc (sub fetch attempted?); stderr:
$(cat 02.get.got.err)"

# --- vendor/sub still intact -----------------------------------------
[ -f vendor/sub/core.c ] \
    || fail "vendor/sub/core.c vanished after second get"
grep -q 'sub_inc' vendor/sub/core.c \
    || fail "vendor/sub/core.c diverged from SUB_C2 after second get:
$(cat vendor/sub/core.c)"

# --- assert SubMount short-circuited the fetch -----------------------
grep -q 'already at pin' 02.get.got.err \
    || fail "expected 'already at pin' marker for sub; stderr:
$(cat 02.get.got.err)"

# --- assert no submodule-fetch-failure diagnostic --------------------
#  Without the fix, WIREFetchAll runs against the moved-aside upstream
#  and SubMount emits this line before returning the failure code.
! grep -q 'submodule fetch failed' 02.get.got.err \
    || fail "submodule fetch was attempted on second get; stderr:
$(cat 02.get.got.err)"

# --- restore so cleanup is clean -------------------------------------
mv "$SUB_BARE.gone" "$SUB_BARE"

note "get/20-sub-already-mounted: second get skipped sub fetch"
