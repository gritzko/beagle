#!/bin/sh
#  get/38-remote-main-branch — DIS-028.  An explicit `?main` against a
#  git remote must resolve the peer's literal `refs/heads/main`, exactly
#  like `?master` already does — instead of failing
#    `fatal: the remote end hung up unexpectedly` / `WIRECLFL` (exit 157).
#
#  Root: be's wire layer aliases be-side trunk (empty branch `""`) to
#  git's default `refs/heads/main` (keeper/WIRECLI.c).  The ref matcher
#  (`wcli_refname_match`) used to translate the ADVERTISED name through
#  the trunk-collapsing `wcli_wire_to_be`, folding `refs/heads/main` to
#  be-side EMPTY.  A literal `?main` want (`want_bare = "main"`, non-
#  empty) then never matched its own advertised ref → no want → fetch
#  aborts WIRECLFL.  Every other branch (`?master`, `?feat/x`) worked;
#  only the literal `main` was unsatisfiable.
#
#  Fix (keeper/WIRECLI.c, wcli_refname_match): parse the advertised name
#  with `GITParseRef` (the raw bare name) instead of `wcli_wire_to_be`,
#  so `refs/heads/main` yields bare `"main"` and matches a literal want.
#  The trunk⇔`main` alias stays only in the empty-want path (HEAD
#  discovery + the fallback loop), so bare-trunk still picks up
#  `refs/heads/main`.
#
#  Hermetic BARE git peer with refs/heads/main AND refs/heads/master at
#  the SAME tip (to show the contrast).  Asserts:
#    (a) `be get …?master`  resolves and checks out   (already worked);
#    (b) `be get …?main`    resolves and checks out    (the DIS-028 fix);
#    (c) bare `be get …`    (no ?ref) maps to refs/heads/main and works.
#
#  Requires passwordless ssh to localhost (gated under WITH_SSH).

. "$(dirname "$0")/../../lib/branches.sh"

#  Keeper's ssh-side path resolution requires $SCRATCH under $HOME.
[ -n "${HOME:-}" ] || fail "get/38: \$HOME unset"
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) fail "get/38: SCRATCH=$SCRATCH not under \$HOME=$HOME" ;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

export GIT_CONFIG_GLOBAL=/dev/null
command -v git >/dev/null 2>&1 || { skip "git not found"; exit 0; }

#  case.sh seeds $SCRATCH/.be as a shield; drop it so the worktrees
#  below bootstrap as their own fresh stores (mirrors post/31).
rmdir "$SCRATCH/.be" 2>/dev/null || true

#  --- 1. bare git peer with refs/heads/main AND refs/heads/master ----
PEER_BARE="$SCRATCH/peer.git"
PEER_URL="ssh://localhost/$REL_SCRATCH/peer.git"
git init -q --bare -b main "$PEER_BARE" \
    || fail "git init --bare peer.git"

SEED="$ETMP/seed"
git init -q -b main "$SEED"
git -C "$SEED" config user.email t@t
git -C "$SEED" config user.name  T
printf 'hello\n' > "$SEED/a.txt"
git -C "$SEED" add -A
git -C "$SEED" commit -qm first
git -C "$SEED" push -q "$PEER_BARE" main:main    || fail "seed push main"
git -C "$SEED" push -q "$PEER_BARE" main:master  || fail "seed push master"

peer_tip=$(git -C "$PEER_BARE" rev-parse main)
[ -n "$peer_tip" ] || fail "peer main not seeded"
[ "$(git -C "$PEER_BARE" rev-parse master)" = "$peer_tip" ] \
    || fail "peer master/main not at the same tip"

#  --- 2. CONTRAST: ?master resolves (pre-existing behaviour) ---------
mkdir wt_master wt_master/.be
( cd wt_master && "$BE" get --nosub "$PEER_URL?master" \
    >01.master.got.out 2>01.master.got.err ) \
    || fail "be get ?master failed: $(cat wt_master/01.master.got.err)"
[ -f wt_master/a.txt ] || fail "?master clone did not check out a.txt"

#  --- 3. THE FIX: ?main resolves the peer's literal refs/heads/main --
#  Pre-fix this exits 157 with WIRECLFL / 'remote end hung up'.
mkdir wt_main wt_main/.be
( cd wt_main && "$BE" get --nosub "$PEER_URL?main" \
    >02.main.got.out 2>02.main.got.err )
rc=$?
[ "$rc" = 0 ] || {
    echo "get/38: be get ${PEER_URL}?main exited $rc (expected 0)" >&2
    echo "  stderr:" >&2; sed 's/^/    /' wt_main/02.main.got.err >&2
    exit 1
}
[ -f wt_main/a.txt ] || fail "?main clone did not check out a.txt"
#  Must NOT have raised the DIS-028 failure signatures.
if grep -qE 'WIRECLFL|hung up unexpectedly' wt_main/02.main.got.err; then
    fail "?main still fails the trunk-alias bug (DIS-028):
$(cat wt_main/02.main.got.err)"
fi

#  --- 4. bare-trunk: no ?ref still maps to refs/heads/main -----------
mkdir wt_bare wt_bare/.be
( cd wt_bare && "$BE" get --nosub "$PEER_URL" \
    >03.bare.got.out 2>03.bare.got.err ) \
    || fail "bare be get (trunk) failed: $(cat wt_bare/03.bare.got.err)"
[ -f wt_bare/a.txt ] || fail "bare-trunk clone did not check out a.txt"

note "get/38-remote-main-branch: ?main resolves refs/heads/main like ?master (tip $peer_tip); bare-trunk still maps to refs/heads/main"
