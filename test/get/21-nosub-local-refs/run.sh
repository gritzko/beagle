#!/bin/sh
#  get/21-nosub-local-refs — repro: `keeper upload-pack` against a
#  healthy be store with multiple project shards destructively
#  mutates each shard's REFS file on open.
#
#  Observed in the wild (gritzko@fi:~/src/beagle, 2026-05-27):
#    * fresh `be get --nosub file:///home/gritzko/src/dogs` (git
#      source → be dest) succeeds; `.be/dogs/refs` is 88 bytes with
#      a real `get` row referencing the source's master tip.
#    * a single `be head be://localhost/src/beagle?/dogs` from a peer
#      fires `keeper upload-pack` on the mount.  After it returns:
#      `.be/dogs/refs` is 4096 bytes of NULs — the original row is
#      gone, the file has been extended to one page, and REFADV has
#      nothing to advertise.  Every subsequent wire request fails
#      with WIREFAIL.
#
#  The clone path is NOT being tested here (it worked).  The
#  regression is purely in keeper's open-for-upload-pack path
#  destructively rewriting REFS files it should only read.
#
#  Seed: two git source repos (mirrors the abc + dogs git sources on
#  fi), cloned into one dest with two project shards.  Test:
#  snapshot every shard's refs, fire one upload-pack invocation,
#  assert byte-exact survival.

. "$(dirname "$0")/../../lib/case.sh"

# Silence git 2.30+ "initial branch name" hint on init.
export GIT_CONFIG_GLOBAL=/dev/null

# case.sh shielded $SCRATCH/.be; we want subdirs to bootstrap as
# their own fresh wts.
rm -rf "$SCRATCH/.be"

# ---------------------------------------------------------------------
# 1. seed TWO git source repos.  -b master keeps git init quiet about
#    the default branch and stable across git versions.
# ---------------------------------------------------------------------
for name in abc dogs; do
    git init -b master "src-$name" >/dev/null 2>&1
    git -C "src-$name" config user.email t@t
    git -C "src-$name" config user.name  T
    cp "$CASE/01.greet.txt" "src-$name/$name.txt"
    git -C "src-$name" add "$name.txt" >/dev/null
    git -C "src-$name" commit -qm "$name: initial"
done

# Capture each source's tip — used to verify the right row landed
# in the dest's shard after clone.
SRC_ABC_TIP=$(git  -C src-abc  rev-parse master | cut -c1-12)
SRC_DOGS_TIP=$(git -C src-dogs rev-parse master | cut -c1-12)
[ -n "$SRC_ABC_TIP" ]  || { echo "FAIL: src-abc tip unreadable"  >&2; exit 1; }
[ -n "$SRC_DOGS_TIP" ] || { echo "FAIL: src-dogs tip unreadable" >&2; exit 1; }

# ---------------------------------------------------------------------
# 2. clone both git sources into ONE dest wt with TWO project shards.
#    Clone 1 from the dest root; clone 2 from a subdir of dest — the
#    second `be get` from the wt root would resolve as a project-
#    switch (the row-0 anchor pins it), so the subdir is the trick
#    that keeps the two shards co-resident under one parent .be/.
#    Mirrors fi:~/src/beagle's clone-from-root + clone-from-subdir.
# ---------------------------------------------------------------------
mkdir dest
( cd dest
  "$BE" get --nosub "file://$SCRATCH/src-abc" \
      > ../02.get-abc.got.out  2> ../02.get-abc.got.err
  mkdir sub
  cd sub
  "$BE" get --nosub "file://$SCRATCH/src-dogs" \
      > ../../03.get-dogs.got.out 2> ../../03.get-dogs.got.err
)

# Sanity: both clones populated their shards with a real refs row.
# (If this trips, the bug isn't in upload-pack — it's earlier, in
# the clone path; see the file history for the original repro.)
for name in abc dogs; do
    shard="dest/.be/src-$name"
    [ -f "$shard/refs" ] || {
        echo "FAIL(setup): $shard/refs missing after clone" >&2
        ls -la "$shard" 2>/dev/null || ls -la dest/.be >&2
        exit 1
    }
    nz=$(tr -d '\000' < "$shard/refs" | wc -c)
    if [ "$nz" -eq 0 ]; then
        echo "FAIL(setup): $shard/refs is all-NUL after clone — clone broken" >&2
        xxd "$shard/refs" | head -3 >&2
        exit 1
    fi
done
grep -q "$SRC_ABC_TIP"  dest/.be/src-abc/refs  || {
    echo "FAIL(setup): src-abc/refs has no row for tip $SRC_ABC_TIP"  >&2
    head -c 400 dest/.be/src-abc/refs >&2; echo >&2; exit 1
}
grep -q "$SRC_DOGS_TIP" dest/.be/src-dogs/refs || {
    echo "FAIL(setup): src-dogs/refs has no row for tip $SRC_DOGS_TIP" >&2
    head -c 400 dest/.be/src-dogs/refs >&2; echo >&2; exit 1
}

# Sanity: the clone must also CHECK OUT the worktree, not just write
# the refs row.  A local `file:///abs/path` clone has no host and no
# `?ref`, so REFSResolve has to wildcard-match the host-less local
# row keeper just wrote; when that funnel breaks, keeper's fetch still
# lands the refs row but sniff's checkout aborts (SNIFFFAIL) and the
# tree is empty.  Assert the source blob bytes actually materialised.
# (Regression for the authority-less file:// resolve gap; the bug was
# silent here because the test only inspected REFS, never the wt.)
for pair in "dest/abc.txt:abc" "dest/sub/dogs.txt:dogs"; do
    wtfile=${pair%%:*}
    [ -f "$wtfile" ] || {
        echo "FAIL(setup): clone did not check out $wtfile" >&2
        find dest -maxdepth 2 ! -path '*/.be/*' >&2
        exit 1
    }
    if ! cmp -s "$CASE/01.greet.txt" "$wtfile"; then
        echo "FAIL(setup): $wtfile content differs from source blob" >&2
        xxd "$wtfile" | head -3 >&2
        exit 1
    fi
done

# ---------------------------------------------------------------------
# 3. THE TEST — `keeper upload-pack` against the healthy dest must
#    leave every shard's REFS byte-exact.
# ---------------------------------------------------------------------
KEEPER=$(dirname "$BE")/keeper
[ -x "$KEEPER" ] || {
    echo "FAIL: cannot find keeper binary alongside be ($KEEPER)" >&2
    exit 1
}

for name in abc dogs; do
    cp "dest/.be/src-$name/refs" "04.$name.refs.before"
done

# </dev/null = client sends nothing; upload-pack emits refs advert,
# drains EOF, exits.  We don't care about its stdout/stderr/exit —
# only the on-disk side effect on the destination's REFS.
"$KEEPER" upload-pack "$SCRATCH/dest" </dev/null \
    > 05.uploadpack.got.out 2> 05.uploadpack.got.err || true

for name in abc dogs; do
    after="06.$name.refs.after"
    before="04.$name.refs.before"
    cp "dest/.be/src-$name/refs" "$after"

    if ! cmp -s "$before" "$after"; then
        sz_b=$(wc -c < "$before")
        sz_a=$(wc -c < "$after")
        echo "FAIL: upload-pack mutated src-$name/refs ($sz_b → $sz_a bytes)" >&2
        echo "--- before ---" >&2; xxd "$before" | head -5 >&2
        echo "--- after  ---" >&2; xxd "$after"  | head -5 >&2
        exit 1
    fi
done
