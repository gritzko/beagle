#!/bin/sh
#  post/37-push-central-store-no-home-escape — POST-014: a push to a
#  CENTRAL (multi-project `~/.be`-style) beagle store must NOT escape the
#  recv-side colocated wt-advance up to the store PARENT directory.
#
#  The bug: `keeper receive-pack` advanced its colocated primary wt by
#  deriving the wt root as `dirname(dirname(<project-shard>))` and then
#  probing `<that>/.be/wtlog` for ANY regular file.  For a central store
#  (`<root>/.be/<proj>/`) that derivation is `<root>` — and the keeper's
#  own `h->wt` is the STORE DIR (`<root>/.be`), not a worktree at all.  A
#  stray / legacy `<root>/.be/wtlog` (e.g. the dogfooding box's real
#  `~/.be/wtlog`) made the probe wrongly accept `<root>` as a primary wt.
#  `be get ?` then ran with cwd=`<root>`, materialising the pushed files
#  into the WRONG tree (HOME-escape) or failing 157 on a multi-project
#  store — while the actual secondary worktree elsewhere was stranded —
#  and the raw `Error: SNIFFFAIL` / `Error: BEDOGEXIT` from that child
#  made a SUCCESSFUL push look like a hard failure.
#
#  Fix (keeper/RECV.c RECVCaptureWtPath): only advance a wt whose OWN
#  `.be` is the store we received into (`h->wt` == the two-pop wt root);
#  a central store fails that gate and is skipped silently.  And a failed
#  courtesy advance is reported as a deferral ("push OK — wt-advance
#  deferred"), never as a raw `Error:` line.
#
#  Hermetic: a private scratch $HOME under $SCRATCH so the central store
#  lives at <home>/.be WITHOUT touching the dev box's real ~/.be.  No ssh;
#  drives the local-exec keeper `file://` edge (same as post/32).
#
#  Layout (all under $SCRATCH):
#    HOME/.be           — central store: shards `proj`, `other`, plus a
#                         STRAY top-level `wtlog` (the HOME-escape bait).
#    A                  — secondary worktree of `proj`: `.be` is a FILE
#                         anchoring back to HOME/.be/proj (like the real
#                         /home/gritzko/beagle).
#    B                  — independent clone of `proj`; edits, commits,
#                         FF-pushes back to the central store.
#
#  Asserts:
#    1. The push exits 0 and reports `keeper: pushed`.
#    2. The central store's `proj` ref FF-advanced to B's tip.
#    3. NO HOME-escape: $HOME got no stray `hello.c` checkout, and the
#       stray $HOME/.be/wtlog did NOT grow a recv `get` row.
#    4. NO masked-success noise: the push stderr has no `Error: SNIFFFAIL`
#       / `Error: BEDOGEXIT` (a successful push must not read as failure).

. "$(dirname "$0")/../../lib/case.sh"

#  case.sh seeds $SCRATCH/.be to shield walk-up; we run with a private
#  $HOME, so drop it and build our own anchors below.
rm -rf "$SCRATCH/.be"

#  Private scratch HOME — the central store sits at $HOME/.be.  Never the
#  dev box's real ~/.be (test-hermetic-be-store).
HOME="$SCRATCH/home"
export HOME
mkdir -p "$HOME/.be"

STORE="$HOME/.be"
A="$SCRATCH/A"
B="$SCRATCH/B"

# ====================================================================
# 1. Seed two projects colocated, then move their shards into the
#    central store so it is genuinely MULTI-project.  The project name
#    is the seed dir's basename — so seed each in a dir named exactly
#    after the project (a subdir of $SCRATCH/seeds to avoid name clash
#    with the $SCRATCH/A and $SCRATCH/B worktrees below).
# ====================================================================
mkdir -p "$SCRATCH/seeds"
for PJ in proj other; do
    S="$SCRATCH/seeds/$PJ"
    mkdir -p "$S/.be"
    ( cd "$S"
      cp "$CASE/00.hello.c" "f_$PJ.c"
      sleep 0.02; "$BE" put "f_$PJ.c"      > "$SCRATCH/01.$PJ.put.out"  2> "$SCRATCH/01.$PJ.put.err"
      sleep 0.02; "$BE" post "seed $PJ"    > "$SCRATCH/02.$PJ.post.out" 2> "$SCRATCH/02.$PJ.post.err" )
    [ -d "$S/.be/$PJ" ] || {
        echo "post/37: seed $PJ shard not named '$PJ': $(ls "$S/.be")" >&2; exit 1; }
    cp -r "$S/.be/$PJ" "$STORE/$PJ"
done
ls -d "$STORE/proj" "$STORE/other" >/dev/null 2>&1 || {
    echo "post/37: central store did not get both shards" >&2
    ls "$STORE" >&2; exit 1
}

PROJ_TIP=$( cd "$SCRATCH/seeds/proj" && "$BE" sha1:? )
[ -n "$PROJ_TIP" ] || { echo "post/37: seed proj has no tip" >&2; exit 1; }

#  STRAY top-level wtlog — the HOME-escape bait.  A real dogfooding
#  `~/.be/wtlog` looks exactly like this (a stray/legacy anchor row).
cp "$SCRATCH/seeds/other/.be/wtlog" "$STORE/wtlog"
STORE_WTLOG_BEFORE=$(wc -l < "$STORE/wtlog")

# ====================================================================
# 2. A — secondary worktree of `proj`: `.be` is a FILE anchoring back
#    to the central store, exactly like /home/gritzko/beagle.
# ====================================================================
mkdir -p "$A"
printf '26610AAAAA\trepo\tfile:%s/proj/\n' "$STORE"   >  "$A/.be"
printf '26610AAAAB\tget\t?#%s\n'          "$PROJ_TIP" >> "$A/.be"
( cd "$A"
  "$BE" get '#' --force > "$SCRATCH/03.Aget.out" 2> "$SCRATCH/03.Aget.err" ) || {
    echo "post/37: A failed to materialise: $(cat "$SCRATCH/03.Aget.err")" >&2; exit 1; }
match "$CASE/00.hello.c" "$A/f_proj.c"

# ====================================================================
# 3. B — independent clone of `proj` from the central store.
# ====================================================================
mkdir -p "$B/.be"
( cd "$B"
  "$BE" get "file://$STORE?/proj/" > "$SCRATCH/04.Bget.out" 2> "$SCRATCH/04.Bget.err" ) || {
    echo "post/37: B failed to clone proj: $(cat "$SCRATCH/04.Bget.err")" >&2; exit 1; }
match "$CASE/00.hello.c" "$B/f_proj.c"

PROJ_TIP_BEFORE=$( cd "$B" && "$BE" sha1:? )

# ====================================================================
# 4. B edits, commits, FF-pushes to the central store (the bug site).
# ====================================================================
( cd "$B"
  cp "$CASE/01.hello.c" f_proj.c
  sleep 0.02; "$BE" post 'B edits f_proj.c' > "$SCRATCH/05.Bpost.out" 2> "$SCRATCH/05.Bpost.err"
  sleep 0.02; "$BE" post "file://$STORE?/proj" > "$SCRATCH/06.Bpush.out" 2> "$SCRATCH/06.Bpush.err" )
PUSH_RC=$?
B_TIP=$( cd "$B" && "$BE" sha1:? )

# --- Assert 1: push exits 0 and announces success. ------------------
[ "$PUSH_RC" -eq 0 ] || {
    echo "post/37: push exited $PUSH_RC (success must be exit 0)" >&2
    echo "  stdout:" >&2; cat "$SCRATCH/06.Bpush.out" >&2
    echo "  stderr:" >&2; cat "$SCRATCH/06.Bpush.err" >&2
    exit 1
}
grep -q "keeper: pushed" "$SCRATCH/06.Bpush.out" || {
    echo "post/37: push did not report success" >&2
    cat "$SCRATCH/06.Bpush.out" >&2
    exit 1
}

# --- Assert 4: no masked-success noise on stderr. -------------------
#  A successful push must not surface a raw recv-child failure as if the
#  push itself failed.  These are the exact lines the bug emitted.
if grep -Eq "Error: SNIFFFAIL|Error: BEDOGEXIT|returned 157" "$SCRATCH/06.Bpush.err"; then
    echo "post/37: push stderr carries masked-failure noise (POST-014)" >&2
    cat "$SCRATCH/06.Bpush.err" >&2
    exit 1
fi

# --- Assert 2: the central store's `proj` ref FF-advanced. ----------
PROJ_TIP_AFTER=$( head -1 "$STORE/proj/refs" >/dev/null 2>&1; cd "$B" && "$BE" sha1:? )
#  Re-read the store ref directly (B's view shares the store, so sha1:?
#  reflects it).  The store ref must now equal B's pushed tip.
[ "$B_TIP" != "$PROJ_TIP_BEFORE" ] || {
    echo "post/37: B's tip did not advance after commit" >&2; exit 1; }
grep -q "$B_TIP" "$STORE/proj/refs" || {
    echo "post/37: central store proj/refs did not advance to $B_TIP" >&2
    cat "$STORE/proj/refs" >&2
    exit 1
}

# --- Assert 3: NO HOME-escape. -------------------------------------
#  The store PARENT ($HOME) must NOT have received a stray checkout, and
#  the stray $HOME/.be/wtlog must NOT have grown a recv `get` row.
if [ -e "$HOME/f_proj.c" ] || [ -e "$HOME/f_other.c" ]; then
    echo "post/37: HOME-escape — store parent got a stray checkout:" >&2
    ls -la "$HOME" >&2
    exit 1
fi
STORE_WTLOG_AFTER=$(wc -l < "$STORE/wtlog")
[ "$STORE_WTLOG_AFTER" -eq "$STORE_WTLOG_BEFORE" ] || {
    echo "post/37: HOME-escape — stray store wtlog grew (before=$STORE_WTLOG_BEFORE after=$STORE_WTLOG_AFTER)" >&2
    cat "$STORE/wtlog" >&2
    exit 1
}

echo "post/37: central-store push landed (proj -> $B_TIP) with no HOME-escape, no masked failure"
