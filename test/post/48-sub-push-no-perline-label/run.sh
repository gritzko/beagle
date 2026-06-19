#!/bin/sh
#  post/47-sub-push-no-perline-label — POST-022 / BE-007 repro.
#
#  A parent `be post file://<peer>?/par` recursing the push into its
#  mounted `vendor/sub` submodule must emit the sub's pushed-difference
#  banner as ONE relayed module hunk with a compact `vendor/sub/` prefix
#  on each row — NOT a bare standalone `vendor/sub` label line per row.
#
#  Pre-fix (ROWS): the sub-push banner emitted each commit/file row as a
#  transient one-row hunk with an EMPTY uri; the parent's HUNKu8sRelay
#  then rebased every empty-uri hunk to the mount path, producing a bare
#  `vendor/sub` line standing alone before each row — pages of wreckage
#  (Fault A).  This test asserts those bare label lines do NOT appear and
#  that the sub rows wear the `vendor/sub/` prefix.

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

cd "$SCRATCH"

# 1. git chain: sub ← par (gitlinks vendor/sub)
mkg sub || { echo "FAIL(setup): git init sub"; exit 1; }
printf 'sub payload v1\n' > sub/core.c
git -C sub add -A; git -C sub commit -qm sub

mkg par || { echo "FAIL(setup): git init par"; exit 1; }
printf 'int main(void) { return 0; }\n' > par/main.c
git -C par -c protocol.file.allow=always submodule add -q "$SCRATCH/sub" vendor/sub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add sub → par"; exit 1; }
git -C par add -A; git -C par commit -qm par

# 2. clone git par into a beagle PEER store B1 (sibling shards par + sub)
mkdir -p B1/.be
( cd B1 && "$BE" get "file:$SCRATCH/par" >../01.b1.out 2>../01.b1.err ) \
    || { cat 01.b1.err >&2; echo "FAIL(setup): clone git par into B1" >&2; exit 1; }

# 3. clone the beagle peer's par project into a working tree B2
mkdir -p B2/.be
( cd B2 && "$BE" get "file://$SCRATCH/B1?/par" >../02.b2.out 2>../02.b2.err ) \
    || { cat 02.b2.err >&2; echo "FAIL(setup): beagle clone of par into B2" >&2; exit 1; }
[ -f B2/vendor/sub/core.c ] \
    || { echo "FAIL(setup): B2 sub not mounted" >&2; exit 1; }

# 4. dirty ONLY the sub; recursive local commit (no push yet)
sleep 0.05
printf '\nvoid sub_extra(void) { }\n' >> B2/vendor/sub/core.c
( cd B2 && "$BE" post '#sync' >../03.post.out 2>../03.post.err ) \
    || { cat 03.post.err >&2; echo "FAIL: recursive local commit" >&2; exit 1; }

# 5. THE PUSH — recurses into vendor/sub; capture the relayed banner.
( cd B2 && "$BE" post "file://$SCRATCH/B1?/par" >../04.push.out 2>../04.push.err )
rc=$?
[ "$rc" = 0 ] || { echo "FAIL: parent push exited $rc" >&2
                   cat 04.push.err >&2; exit 1; }

out=04.push.out
dump() { echo "--- $out was: ---" >&2; cat "$out" >&2; }

# 6. POST-022 Fault A: NO bare standalone `vendor/sub` label line.
#    Strip trailing CR/space, then reject any line that is exactly the
#    mount path with nothing else on it.
if grep -Eq '^[[:space:]]*vendor/sub[[:space:]]*$' "$out"; then
    echo "post/47: bare 'vendor/sub' label line present (POST-022 wreckage)" >&2
    dump; exit 1
fi

# 7. The sub's pushed rows must still appear, carrying the mount prefix
#    (a row mentioning vendor/sub/<something>, e.g. the touched file or a
#    prefixed commit row).  Bounded, prefixed output — not a bare label.
grep -Eq 'vendor/sub/' "$out" \
    || { echo "post/47: sub rows lost their 'vendor/sub/' prefix" >&2; dump; exit 1; }

echo "post/47-sub-push-no-perline-label: sub-push banner is one prefixed hunk, no per-row label"
