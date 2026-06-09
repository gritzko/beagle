#!/bin/sh
#  get/39-be-pathless-project — GET-003.  The keeper wire selects the
#  project with a `?/<project>` QUERY selector and an EMPTY path: the
#  empty path denotes the peer's default store, the query picks the
#  shard within it (`be get be://host?/proj`, `be head be://host?/proj`).
#
#  Pre-fix `wcli_spawn` (keeper/WIRECLI.c) rejected an empty path with
#  WIRECLFL *before* the scheme was classified — so the natural keeper
#  clone form (project in the query, default store, no repo path) died
#  WIRECLFL / BEDOGEXIT (exit 157), blocking the be:// fix-and-land loop.
#
#  Fix: the empty-path rejection is now scheme-specific (after
#  classification).  A keeper peer (keeper://local | be://host) with a
#  `?/<project>` query accepts an empty path and serves a bare `?/proj`
#  argv; the server's keeper_served_at + home_open_inner walk-up resolve
#  it against the peer's default store.  Git transports (ssh/https/git)
#  still require a path (git-upload-pack <path>), so empty-path → WIRECLFL
#  stays for them.
#
#  Hermetic via the LOCAL-EXEC keeper transport (keeper://local) — no ssh
#  needed, and the spawned keeper inherits be's cwd so its `.be` walk-up
#  lands on a store WE control (never $HOME/.be).  This exercises the
#  exact broken client path (empty path + `?/proj` query → keeper exec).
#  The be:// ssh-keeper arm shares this client code; over ssh it resolves
#  against the remote login's $HOME/.be (untestable hermetically).

. "$(dirname "$0")/../../lib/branches.sh"

#  case.sh seeded $SCRATCH/.be as a shield; drop it so the dirs below
#  bootstrap as their own fresh stores.
rmdir "$SCRATCH/.be" 2>/dev/null || true

PROJ=widget

#  --- 1. Seed a hermetic source store holding project `$PROJ` ---------
SRC="$SCRATCH/src"
mkdir -p "$SRC/.be"
( cd "$SRC"
  printf 'pathless clone payload\n' > a.txt
  "$BE" put a.txt   >put.out  2>put.err
  "$BE" post 'seed pathless project' >post.out 2>post.err ) \
    || fail "seed source project"
#  Project (Title) defaults to the wt basename → shard `.be/src`.
SHARD=$(ls "$SRC/.be" 2>/dev/null | grep -vE 'wtlog|config|idx|^refs$' | head -1)
[ -n "$SHARD" ] && [ -d "$SRC/.be/$SHARD" ] || fail "source shard not created"

#  Snapshot of the source store (full `.be`: shard objects + store-level
#  refs/wtlog/config).  Cloning into a fresh dir holding this snapshot —
#  but no worktree files — forces the wire round-trip + a real checkout.
SBE="$SRC/.be"

#  --- 2. be head keeper://local?/<shard> — pathless query selector ----
#  The local-exec keeper inherits the cwd's `.be` walk-up, so seed a
#  store snapshot and run from it.  Pre-fix: exit 157, "Error: WIRECLFL".
#  Post-fix: exit 0, head printed (no empty-path rejection).
HD="$SCRATCH/hd"
mkdir -p "$HD/.be"
cp -r "$SBE/." "$HD/.be/"
rc=0
( cd "$HD" && "$BE" head "keeper://local?/$SHARD" \
    >01.head.got.out 2>01.head.got.err ) || rc=$?
[ "$rc" = 0 ] || {
    echo "get/39: be head keeper://local?/$SHARD exited $rc (expected 0)" >&2
    echo "  stderr:" >&2; sed 's/^/    /' "$HD/01.head.got.err" >&2
    exit 1
}
if grep -q 'WIRECLFL' "$HD/01.head.got.err"; then
    fail "be head still fails the empty-path guard (GET-003):
$(cat "$HD/01.head.got.err")"
fi

#  --- 3. be get keeper://local?/<shard> — clones the worktree --------
#  Fresh store snapshot, NO worktree files; the clone must populate the
#  worktree via the wire negotiation (advertise → want → pack → checkout).
GW="$SCRATCH/gw"
mkdir -p "$GW/.be"
cp -r "$SBE/." "$GW/.be/"
rc=0
( cd "$GW" && "$BE" get --nosub "keeper://local?/$SHARD" \
    >02.get.got.out 2>02.get.got.err ) || rc=$?
[ "$rc" = 0 ] || {
    echo "get/39: be get keeper://local?/$SHARD exited $rc (expected 0)" >&2
    echo "  stderr:" >&2; sed 's/^/    /' "$GW/02.get.got.err" >&2
    exit 1
}
[ -f "$GW/a.txt" ] || fail "pathless clone did not check out a.txt"
match "$SRC/a.txt" "$GW/a.txt"
if grep -q 'WIRECLFL' "$GW/02.get.got.err"; then
    fail "be get still fails the empty-path guard (GET-003):
$(cat "$GW/02.get.got.err")"
fi

#  --- 4. NEGATIVE: keeper-local empty path AND no query → WIRECLFL ----
#  Nothing to serve: with neither a path nor a `?/proj` selector there
#  is no shard to route to, so the keeper branch must still reject it.
NW="$SCRATCH/nw"
mkdir -p "$NW/.be"
cp -r "$SBE/." "$NW/.be/"
rc=0
( cd "$NW" && "$BE" head "keeper://local" \
    >03.noq.got.out 2>03.noq.got.err ) || rc=$?
[ "$rc" != 0 ] || fail "keeper://local with no path and no query unexpectedly succeeded"
grep -q 'WIRECLFL' "$NW/03.noq.got.err" \
    || fail "keeper://local (no path, no query) must error WIRECLFL; got:
$(cat "$NW/03.noq.got.err")"

#  --- 5. NEGATIVE: git transport empty path → WIRECLFL ---------------
#  git-upload-pack needs a repo path; the keeper-only empty-path relax
#  must NOT leak to ssh/https/git (DOGIsGitTransport classifies ssh).
#  No ssh connection is attempted — wcli_spawn rejects before spawn.
GN="$SCRATCH/gn"
mkdir -p "$GN/.be"
cp -r "$SBE/." "$GN/.be/"
rc=0
( cd "$GN" && "$BE" head "ssh://localhost?/$SHARD" \
    >04.git.got.out 2>04.git.got.err ) || rc=$?
[ "$rc" != 0 ] || fail "git-transport (ssh) empty path unexpectedly succeeded"
grep -q 'WIRECLFL' "$GN/04.git.got.err" \
    || fail "git-transport empty path must error WIRECLFL; got:
$(cat "$GN/04.git.got.err")"

note "get/39-be-pathless-project: keeper://local?/$SHARD (empty path, ?/proj query) resolves head + clones; git-transport + selector-less keeper empty path still WIRECLFL"
