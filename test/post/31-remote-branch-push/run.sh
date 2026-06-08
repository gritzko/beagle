#!/bin/sh
#  post/31-remote-branch-push — DIS-026.  `be post //git-remote?<branch>`
#  must push cur's tip onto the peer's `refs/heads/<branch>` (FF), with
#  `?<branch>` read as the REMOTE target — NEVER requiring a local label.
#
#  Per [POST] the slot model: `//remote` FF-pushes cur's tip and
#  `?branch` names the remote counterpart that push advances.  So
#  `be post ssh://…/<peer>?master` SHOULD push cur onto the peer's
#  `master`.  Before the fix it didn't: sniff post read `?master` as a
#  required LOCAL label and refused with
#    `sniff: post: ?master does not exist — be put ?<branch> first`
#  / `NOBRANCH` / `BEDOGEXIT` (exit 157), so the keeper wire push never
#  ran and the peer ref never moved.
#
#  Fix (sniff/SNIFF.exe.c, post-verb dispatch): a `?branch` riding a
#  transport authority is the REMOTE push target (the wire refname),
#  not a local FF-promote target.  Sniff (push-agnostic) skips it; the
#  `keeper post` stage (BEActKeeperPush) reads `?master` as the wire
#  refname and FF-pushes cur there.  DIS-019 stays intact: a synthetic
#  dot-coordinate is forbidden only as the *wire refname*; an explicit
#  `?<branch>` IS the refname, so cur is a legal SOURCE — push it.
#
#  Distinguish from the LOCAL `?branch` case: `be post ?nonexistent`
#  with NO authority must STILL refuse NOBRANCH (DIS-020) — the fix only
#  changes the authority-bearing form.
#
#  Requires passwordless ssh to localhost (gated under WITH_SSH).

. "$(dirname "$0")/../../lib/branches.sh"

#  Keeper's ssh-side path resolution requires $SCRATCH under $HOME.
[ -n "${HOME:-}" ] || fail "post/31: \$HOME unset"
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) fail "post/31: SCRATCH=$SCRATCH not under \$HOME=$HOME" ;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

export GIT_CONFIG_GLOBAL=/dev/null
command -v git >/dev/null 2>&1 || { skip "git not found"; exit 0; }

#  case.sh seeds $SCRATCH/.be/ as a shield; drop it so wt/ is a clean
#  primary worktree (mirrors submodules.sh).
rmdir "$SCRATCH/.be" 2>/dev/null || true

#  --- 1. bare git peer with one commit on master ---------------------
PEER_BARE="$SCRATCH/peer.git"
PEER_URL="ssh://localhost/$REL_SCRATCH/peer.git"
git init -q --bare -b master "$PEER_BARE" \
    || fail "git init --bare peer.git"
#  A bare repo refuses nothing odd here, but be explicit so a non-bare
#  checkout would also accept the FF.
git -C "$PEER_BARE" config receive.denyCurrentBranch ignore

SEED="$ETMP/seed"
git init -q -b master "$SEED"
git -C "$SEED" config user.email t@t
git -C "$SEED" config user.name  T
printf 'hello\n' > "$SEED/a.txt"
git -C "$SEED" add -A
git -C "$SEED" commit -qm first
git -C "$SEED" push -q "$PEER_BARE" master:master \
    || fail "seed push to peer"

peer_before=$(git -C "$PEER_BARE" rev-parse master)
[ -n "$peer_before" ] || fail "peer master not seeded"

#  --- 2. clone the git peer into a beagle worktree -------------------
mkdir wt wt/.be && cd wt
"$BE" get --nosub "$PEER_URL?master" >01.get.got.out 2>01.get.got.err \
    || fail "be get peer failed: $(cat 01.get.got.err)"
[ -f a.txt ] || fail "clone did not check out a.txt"

#  --- 3. commit a descendant on top (clean FF over peer's master) ----
sleep 0.02
printf 'world\n' >> a.txt
"$BE" put a.txt >02.put.got.out 2>02.put.got.err \
    || fail "be put a.txt: $(cat 02.put.got.err)"
"$BE" post '#second' >03.post.got.out 2>03.post.got.err \
    || fail "be post (local) failed: $(cat 03.post.got.err)"
cur=$(head_hex)
[ -n "$cur" ] || fail "no cur tip after local commit"
[ "$cur" != "$peer_before" ] || fail "local commit did not advance cur"

#  --- 4. THE CHECK: be post //git-remote?master ----------------------
#  Must FF-advance the peer's refs/heads/master to cur and exit 0.
"$BE" post "$PEER_URL?master" >04.push.got.out 2>04.push.got.err
rc=$?

#  (a) MUST exit clean — no NOBRANCH / BEDOGEXIT.
[ "$rc" = 0 ] || {
    echo "post/31: be post ${PEER_URL}?master exited $rc (expected 0)" >&2
    echo "  stdout:" >&2; sed 's/^/    /' 04.push.got.out >&2
    echo "  stderr:" >&2; sed 's/^/    /' 04.push.got.err >&2
    exit 1
}

#  (b) MUST NOT have raised NOBRANCH / 'does not exist' (the bug).
if grep -q 'does not exist' 04.push.got.err; then
    fail "remote ?master misread as a missing LOCAL label (NOBRANCH bug):
$(cat 04.push.got.err)"
fi
if grep -q 'NOBRANCH' 04.push.got.err; then
    fail "NOBRANCH surfaced for a remote push target:
$(cat 04.push.got.err)"
fi

#  (c) the peer's refs/heads/master advanced to cur.
peer_after=$(git -C "$PEER_BARE" rev-parse master)
[ "$peer_after" = "$cur" ] || {
    echo "post/31: peer master did not advance to cur" >&2
    echo "  before: $peer_before" >&2
    echo "  cur   : $cur" >&2
    echo "  after : $peer_after" >&2
    echo "  push stderr:" >&2; sed 's/^/    /' 04.push.got.err >&2
    exit 1
}

#  --- 5. NEGATIVE: local ?nonexistent (no authority) still refuses ---
#  The fix must NOT relax DIS-020: a missing LOCAL branch target with no
#  transport authority is still a hard NOBRANCH error.
if "$BE" post '?nonexistent' >05.local.got.out 2>05.local.got.err; then
    fail "local ?nonexistent unexpectedly succeeded — fix over-skipped:
$(cat 05.local.got.err)"
fi
grep -q 'does not exist' 05.local.got.err \
    || fail "local ?nonexistent did not refuse 'does not exist':
$(cat 05.local.got.err)"

note "post/31-remote-branch-push: remote ?master pushed cur to peer ($peer_before -> $cur); local ?nonexistent still refuses NOBRANCH"
