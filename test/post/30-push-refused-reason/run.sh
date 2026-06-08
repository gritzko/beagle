#!/bin/sh
#  post/30-push-refused-reason — when a git peer's receive-pack REFUSES a
#  push, `be post //remote` must SURFACE the peer's own reason, not bury
#  it under an opaque WIRECLFL / "the remote end hung up unexpectedly".
#
#  Spec (https://replicated.wiki/html/wiki/POST.html §POST): a `//remote`
#  push that fails is reported with the peer's diagnostic so the cause is
#  knowable from be's output.  See todo/DIS/DIS-027.mkd.
#
#  Setup (a deterministic refusal that is NOT a client-side non-FF — the
#  push is a clean fast-forward, so it reaches the wire and is refused by
#  the peer's receive-pack, exercising the report-status `ng` + side-band-2
#  relay path, not the local WIRECLNFF gate):
#    A           — origin's seed on master.
#    B (client)  — one commit on top of A in the wt (a clean FF of origin).
#  origin installs a `pre-receive` hook that prints a KNOWN MARKER to
#  stderr and exits 1, so receive-pack declines every ref update.
#
#  Asserts:
#    * `be post //localhost` exits non-zero (the push genuinely failed).
#    * be's stderr CONTAINS the peer's marker AND/OR the `ng` reason
#      ("pre-receive hook declined") — the cause is surfaced, not swallowed.
#
#  Requires passwordless ssh to localhost (gated under WITH_SSH).

. "$(dirname "$0")/../../lib/case.sh"

[ -n "${HOME:-}" ] || { echo "post/30: \$HOME unset" >&2; exit 1; }
case "$SCRATCH" in
    "$HOME"/*) ;;
    *) echo "post/30: SCRATCH=$SCRATCH not under \$HOME=$HOME" >&2
       echo "         keeper's ssh-side path resolution requires it" >&2
       exit 1;;
esac
REL_SCRATCH=${SCRATCH#$HOME/}

MARKER="DIS027-REFUSED-MARKER"
ORIGIN="$SCRATCH/origin.git"
SEED="$SCRATCH/seed"
REL_ORIGIN="$REL_SCRATCH/origin.git"

cd "$SCRATCH"

# ====================================================================
# 1. seed origin with version A on master
# ====================================================================
git init --bare "$ORIGIN" >/dev/null
git init "$SEED" >/dev/null
git -C "$SEED" config user.email t@t
git -C "$SEED" config user.name  T
git -C "$SEED" checkout -b master >/dev/null || true
sleep 0.02; cp "$CASE/01.A.hello.c" "$SEED/hello.c"
git -C "$SEED" add . >/dev/null
git -C "$SEED" commit -qm A
git -C "$SEED" push -q "$ORIGIN" master:master

# ====================================================================
# 2. install a pre-receive hook on origin that ALWAYS refuses with a
#    known marker on stderr.  receive-pack relays hook stderr on
#    side-band-2 ("remote: <line>") and reports `ng <ref> pre-receive
#    hook declined`.
# ====================================================================
cat > "$ORIGIN/hooks/pre-receive" <<EOF
#!/bin/sh
echo "error: $MARKER" >&2
exit 1
EOF
chmod +x "$ORIGIN/hooks/pre-receive"

# ====================================================================
# 3. clone into wt via ssh
# ====================================================================
mkdir wt wt/.be && cd wt   # shield from $HOME home repo (CLAUDE.md)
"$BE" get "ssh://localhost/$REL_ORIGIN?master" \
    >01.clone.got.out 2>01.clone.got.err
match "$CASE/01.A.hello.c" hello.c

# ====================================================================
# 4. local commit on cur (parent = A) — a clean fast-forward of origin
# ====================================================================
sleep 0.02; cp "$CASE/02.client.hello.c" hello.c
"$BE" post 'client edits' >02.post.got.out 2>02.post.got.err
match "$CASE/02.client.hello.c" hello.c

# ====================================================================
# 5. `be post //localhost` — a clean FF push, but the peer's
#    pre-receive hook declines.  MUST exit non-zero AND surface the
#    peer's reason on stderr.
# ====================================================================
if "$BE" post "//localhost" >03.post.got.out 2>03.post.got.err; then
    echo "post/30: be post //localhost unexpectedly SUCCEEDED" >&2
    cat 03.post.got.err >&2
    exit 1
fi

#  The peer's reason must be visible.  Two independent channels carry
#  it; both must be surfaced:
#    (a) the report-status `ng <ref> <reason>` line — the IN-BAND, wire
#        reason ("pre-receive hook declined") that be parses directly.
#        This is the part DIS-027 dropped (only trace()'d), and it is
#        transport-independent (works over keeper:// local exec too,
#        where there is no ssh fd-2 passthrough).
#    (b) the peer's own diagnostic text — the hook's "$MARKER" — relayed
#        over side-band-2 so the human-authored cause survives.
#  An opaque WIRECLFL / "remote end hung up" with neither is the bug.

#  (a) the in-band `ng` reason — the load-bearing assertion.
if ! grep -qi 'pre-receive hook declined' 03.post.got.err; then
    echo "post/30: be dropped the report-status \`ng\` reason." >&2
    echo "         expected 'pre-receive hook declined' on stderr; got:" >&2
    sed 's/^/         /' 03.post.got.err >&2
    exit 1
fi

#  (b) the peer's own marker text (relayed over side-band-2).
if ! grep -q "$MARKER" 03.post.got.err; then
    echo "post/30: be dropped the peer's side-band-2 diagnostic." >&2
    echo "         expected '$MARKER' on stderr; got:" >&2
    sed 's/^/         /' 03.post.got.err >&2
    exit 1
fi

echo "  - post/30: refused push surfaced both the \`ng\` reason and marker" >&2
