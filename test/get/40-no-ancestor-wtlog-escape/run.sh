#!/bin/sh
#  get/40-no-ancestor-wtlog-escape — GET-006.
#
#  `be get` from a brand-new directory with NO `.be` of its own must
#  NOT let store discovery walk UP and write a `wtlog` into an ancestor
#  store.  Running a fresh-dir clone from a subdir of someone else's
#  worktree used to compose the downstream `sniff get`'s `--at` from
#  CLIParse's cwd-walk, which resolved to the ANCESTOR store; the get
#  then opened the ancestor rw and appended its `get` row into the
#  ancestor's `.be/wtlog`, self-anchoring / poisoning it (subsequent
#  ops there fail SNIFFFAIL / "no baseline").  See [[GET-006]].
#
#  FIX (BEActWorktreeAnchor, beagle/BE.cli.c): after BEGetWorktree
#  wires the destination's own `<cwd>/.be` secondary anchor, re-target
#  c->repo + the `--at` URI at cwd so the downstream get opens THIS
#  worktree (whose anchor redirects h->root to the SHARED store and
#  whose `get` row lands in cwd's own `.be`) — never the ancestor.
#
#  HERMETIC: the FAKE ancestor store lives entirely inside $SCRATCH;
#  the fresh clone dir is a subdir of it, so discovery's walk-up hits
#  the FAKE store's `.be` first and can NEVER reach the dev box's real
#  $HOME/.be.  case.sh's hermetic firewall (an empty `.be` FILE at the
#  scratch base) contains any walk that escapes the fake store.  As a
#  belt-and-suspenders check we also assert the real $HOME/.be/wtlog
#  (when present) is byte-identical before/after.

. "$(dirname "$0")/../../lib/case.sh"

#  case.sh shielded $SCRATCH/.be; drop it so the dirs below bootstrap
#  as their own fresh stores.
rm -rf "$SCRATCH/.be"

# ---------------------------------------------------------------------
# 0. Snapshot the real $HOME/.be/wtlog (if any) — must be untouched.
# ---------------------------------------------------------------------
REAL_WTLOG="$HOME/.be/wtlog"
REAL_BEFORE=""
[ -f "$REAL_WTLOG" ] && REAL_BEFORE=$(sha256sum "$REAL_WTLOG" | cut -d' ' -f1)

# ---------------------------------------------------------------------
# 1. FAKE ancestor store with a seed commit (single project shard +
#    a populated wtlog) — the store a buggy walk-up would poison.
# ---------------------------------------------------------------------
mkdir -p store/.be          # empty-.be shield → bootstraps in place
(
    cd store
    printf 'hello\n' > seed.txt
    "$BE" put seed.txt >/dev/null 2>&1
    "$BE" post '#seed'  >/dev/null 2>&1
) || { echo "FAIL(setup): fake ancestor store seed failed" >&2; exit 1; }
[ -d store/.be ]       || { echo "FAIL(setup): store/.be should be a dir" >&2; exit 1; }
[ -f store/.be/wtlog ] || { echo "FAIL(setup): store/.be/wtlog missing" >&2; exit 1; }

ANC_WTLOG="$SCRATCH/store/.be/wtlog"
ANC_BEFORE=$(sha256sum "$ANC_WTLOG" | cut -d' ' -f1)
cp "$ANC_WTLOG" "$SCRATCH/anc.wtlog.before"

# ---------------------------------------------------------------------
# 2. `be get` from a FRESH subdir of the store (no `.be` of its own).
#    Discovery walks up to store/.be; the clone must wire THIS dir as a
#    secondary worktree, not pollute the ancestor.
# ---------------------------------------------------------------------
mkdir -p store/freshclone
(
    cd store/freshclone
    "$BE" get "file:$SCRATCH/store" > get.out 2> get.err
) || { cat store/freshclone/get.err >&2; echo "FAIL: be get file:<store> from fresh subdir" >&2; exit 1; }

# ---------------------------------------------------------------------
# 3. ASSERT — ancestor wtlog byte-identical; clone still functional.
# ---------------------------------------------------------------------
ANC_AFTER=$(sha256sum "$ANC_WTLOG" | cut -d' ' -f1)
if [ "$ANC_BEFORE" != "$ANC_AFTER" ]; then
    echo "FAIL: ancestor store/.be/wtlog was modified (escape)" >&2
    diff -u "$SCRATCH/anc.wtlog.before" "$ANC_WTLOG" >&2 || true
    exit 1
fi

#  The fresh dir must have become its OWN worktree: `.be` a regular
#  FILE (secondary anchor), and the seed file checked out.
[ -f store/freshclone/.be ] && [ ! -d store/freshclone/.be ] \
    || { echo "FAIL: freshclone/.be must be a regular file (anchor)" >&2; ls -la store/freshclone >&2; exit 1; }
match store/seed.txt store/freshclone/seed.txt

#  The clone's own `get` row must live in ITS anchor, not the ancestor.
grep -qF '	get	?' store/freshclone/.be \
    || { echo "FAIL: freshclone/.be should carry the get row" >&2; cat -v store/freshclone/.be >&2; exit 1; }

# ---------------------------------------------------------------------
# 4. Belt-and-suspenders: the real $HOME/.be/wtlog is untouched.
# ---------------------------------------------------------------------
if [ -n "$REAL_BEFORE" ]; then
    REAL_AFTER=$(sha256sum "$REAL_WTLOG" | cut -d' ' -f1)
    [ "$REAL_BEFORE" = "$REAL_AFTER" ] \
        || { echo "FAIL: real \$HOME/.be/wtlog was modified!" >&2; exit 1; }
fi

echo "get/40-no-ancestor-wtlog-escape: OK"
