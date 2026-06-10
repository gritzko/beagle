#!/bin/sh
#  post/37-sub-push-beagle-peer-shard — SUBS-017 repro.
#
#  A parent `be post file://<peer>?/<parent>` (BEAGLE peer) recurses the
#  push into each mounted sub.  The sub sits DETACHED at the gitlink pin
#  and a prior recursive commit parked it on the synthetic coordinate
#  `?/<subproj>/.<parent>/<branch>` (Submodules.mkd §"Committing detached
#  subs").  The sub push MUST land on the sub's OWN sibling shard
#  `?/<subproj>` on the SAME peer.
#
#  Bug (SUBS-017): the parent forwarded the bare store locator (no
#  `?/<subproj>` selector) to the sub, so the sub's `be post <locator>`
#  resolved no project query and the peer routed the commit to its row-0
#  DEFAULT project (the parent's shard) under the synthetic dot-branch —
#  the wrong target.  Two compounding defects:
#    1. no `?/<subproj>` selector → wrong shard (parent's, not sub's);
#    2. the locator was built `scheme + "://" + authority`, double-
#       emitting the `//` for a `file:///abs` URI (authority lexes as
#       `//`) → `file://///abs`, which broke the push (WIRECLFL/exit 157).
#  Fix (beagle/BE.cli.c BEActSubsPost / bepush_sub_uri): compose the
#  locator via URIMake (well-formed slashes) and re-attach each sub's
#  own `?/<subproj>` selector — mirrors the GET-side
#  subs_candidate_from_source (sniff/SUBS.c).
#
#  Hermetic & fully local (file://, no ssh).  A single beagle peer store
#  B1 holds BOTH the parent project and the sub's project as sibling
#  shards (built by cloning a git parent+sub chain into it).  A working
#  tree B2 is cloned from the peer; the sub mounts on its synthetic
#  coordinate.  Then: dirty the sub, recursive commit, parent push.
#  Asserts the sub shard advanced to the sub's local tip and the parent
#  shard advanced to the parent's, with NO synthetic dot-branch pollution
#  on the parent shard.

. "$(dirname "$0")/../../lib/case.sh"
export GIT_CONFIG_GLOBAL=/dev/null

#  case.sh shielded $SCRATCH/.be; drop it so the dirs below bootstrap as
#  their own fresh stores.
rm -rf "$SCRATCH/.be"

command -v git >/dev/null 2>&1 || { echo "SKIP: git not found" >&2; exit 0; }

mkg() {  # $1 = repo dir — quiet init with file:// submodules allowed
    git init -q -b master "$1" >/dev/null 2>&1 || return 1
    git -C "$1" config user.email t@t
    git -C "$1" config user.name  T
    git -C "$1" config protocol.file.allow always
}

tip_of() {  # $1 = .be / wtlog path — last get/post/patch row's 40-hex
    awk -F'\t' '$2=="get"||$2=="post"||$2=="patch" { last=$3 }
        END { h=last; if (h ~ /#/) sub(/^.*#/,"",h);
              else sub(/^[^?]*\?/,"",h); print h }' "$1"
}

# ---------------------------------------------------------------------
# 1. git chain: sub (project "sub") ← par (gitlinks vendor/sub)
# ---------------------------------------------------------------------
mkg sub || { echo "FAIL(setup): git init sub"; exit 1; }
printf 'sub payload v1\n' > sub/core.c
git -C sub add -A; git -C sub commit -qm sub

mkg par || { echo "FAIL(setup): git init par"; exit 1; }
printf 'int main(void) { return 0; }\n' > par/main.c
git -C par -c protocol.file.allow=always submodule add -q "$SCRATCH/sub" vendor/sub >/dev/null 2>&1 \
    || { echo "FAIL(setup): submodule add sub → par"; exit 1; }
git -C par add -A; git -C par commit -qm par

# ---------------------------------------------------------------------
# 2. clone git `par` into a beagle PEER store B1 → sibling shards
#    par + sub, both checked out.  This is the peer that holds BOTH
#    projects (the SUBS-017 precondition).
# ---------------------------------------------------------------------
mkdir -p B1/.be
( cd B1 && "$BE" get "file:$SCRATCH/par" >../01.b1.out 2>../01.b1.err ) \
    || { cat 01.b1.err >&2; echo "FAIL(setup): clone git par into B1" >&2; exit 1; }
for p in par sub; do
    [ -f "B1/.be/$p/refs" ] \
        || { echo "FAIL(setup): B1 missing project shard '$p'" >&2
             ls -R B1/.be 2>/dev/null | head -40 >&2; exit 1; }
done

# ---------------------------------------------------------------------
# 3. clone the BEAGLE peer's `par` project into a working tree B2
#    (transport file:// → keeper).  The sub mounts on its synthetic
#    coordinate, detached at the pin.
# ---------------------------------------------------------------------
mkdir -p B2/.be
( cd B2 && "$BE" get "file://$SCRATCH/B1?/par" >../02.b2.out 2>../02.b2.err ) \
    || { cat 02.b2.err >&2; echo "FAIL(setup): beagle clone of par into B2" >&2; exit 1; }
[ -f B2/vendor/sub/core.c ] \
    || { echo "FAIL(setup): B2 sub not mounted" >&2; exit 1; }

# ---------------------------------------------------------------------
# 4. dirty ONLY the sub; recursive local commit (no push yet).  Parks
#    the sub on its synthetic branch `?/sub/.par/master`.
# ---------------------------------------------------------------------
sleep 0.02
cat >> B2/vendor/sub/core.c <<'EOF'

void sub_extra(void) { }
EOF
( cd B2 && "$BE" post '#sync' >../03.post.out 2>../03.post.err ) \
    || { cat 03.post.err >&2; echo "FAIL: recursive local commit" >&2; exit 1; }

sub_local=$(tip_of B2/vendor/sub/.be)
par_local=$(tip_of B2/.be/wtlog)
[ -n "$sub_local" ] || { echo "FAIL: no sub local tip" >&2; exit 1; }
[ -n "$par_local" ] || { echo "FAIL: no par local tip" >&2; exit 1; }

#  Peer shard tips BEFORE the push (must advance to the locals).
sub_peer_pre=$(tip_of B1/.be/sub/refs)
par_peer_pre=$(tip_of B1/.be/par/refs)

# ---------------------------------------------------------------------
# 5. THE PUSH — `be post file://<peer>?/par`.  The sub push MUST land on
#    the sub's OWN shard `?/sub`, NOT the peer's default (par) shard.
#    Pre-fix: exits 157 (WIRECLFL) and/or pollutes the par shard.
# ---------------------------------------------------------------------
( cd B2 && "$BE" post "file://$SCRATCH/B1?/par" >../04.push.out 2>../04.push.err )
rc=$?
[ "$rc" = 0 ] || { echo "FAIL: parent push exited $rc (SUBS-017: sub push to wrong target)" >&2
                   echo "  stderr:" >&2; cat 04.push.err >&2; exit 1; }

# ---------------------------------------------------------------------
# 6. Assertions: each project's OWN shard advanced to its OWN local tip.
# ---------------------------------------------------------------------
sub_peer_post=$(tip_of B1/.be/sub/refs)
par_peer_post=$(tip_of B1/.be/par/refs)

[ "$sub_peer_post" = "$sub_local" ] \
    || { echo "FAIL: sub shard did not advance to sub's tip" >&2
         echo "  want=$sub_local got=$sub_peer_post (pre=$sub_peer_pre)" >&2
         echo "  par shard refs:" >&2; cat B1/.be/par/refs >&2
         echo "  sub shard refs:" >&2; cat B1/.be/sub/refs >&2; exit 1; }

[ "$par_peer_post" = "$par_local" ] \
    || { echo "FAIL: par shard did not advance to parent's tip" >&2
         echo "  want=$par_local got=$par_peer_post (pre=$par_peer_pre)" >&2; exit 1; }

#  The sub's synthetic dot-branch must NOT have landed on the parent's
#  shard (the precise SUBS-017 mis-route).
if grep -q '?/sub/\.par' B1/.be/par/refs; then
    echo "FAIL: par shard polluted with sub's synthetic dot-branch (SUBS-017 mis-route)" >&2
    cat B1/.be/par/refs >&2
    exit 1
fi

echo "post/37-sub-push-beagle-peer-shard: parent push carried the sub to its OWN shard"
