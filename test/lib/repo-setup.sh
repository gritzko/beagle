# test/lib/repo-setup.sh — THE shared repo-setup procedure for tests.
#
# Maintainer directive (2026-06-03): "Tests must have their repo-setup
# lib procedure; all use it."  There is exactly ONE isolated-store
# setup here; every test (beagle be-*, the data-driven verb/case
# runners, and the sniff/graf/keeper dog scripts) routes its `.be`
# bootstrap through it.  Do NOT hand-roll `mkdir -p <wt>/.be` in a
# test — call rs_fresh_wt / rs_fresh_norepo.
#
# --- WHY isolation is needed -------------------------------------------
#   `be`/`sniff` discover their store by walking the cwd up looking for
#   a `.be`.  On a developer/CI box `$HOME/.be` is a REAL multi-project
#   store.  If a test's scratch worktree has no `.be` of its own (or a
#   `.be` the walk does not treat as an anchor), discovery ESCAPES up to
#   `$HOME/.be`, and the test then sees real-repo state: stray `unk`
#   files (→ SNIFFnorepo false fail), RwBootstrap/cwd-walk surprises
#   (→ HOMEtest), or a full $HOME-tree enumeration that reads as a hang.
#
# --- HOW this lib achieves isolation -----------------------------------
#   Two complementary modes (see dog/HOME.c::home_walk_up):
#
#     rs_fresh_wt — a REPO worktree.  Seeds an EMPTY `.be/` dir in the
#       scratch wt.  A `.be/` dir with NO `wtlog` and NO project-shard
#       subdirs is a worktree-shield / fresh-bootstrap anchor: the walk
#       STOPS there instead of ascending to `$HOME/.be`.  The first
#       `be`/`sniff` command bootstraps it into a real store in-place.
#       Rooted under $HOME (ext4) so MAP_SHARED store mmaps work and the
#       WITH_SSH wire cases get keeper's ssh-side path resolution.
#
#     rs_fresh_norepo — a genuine NO-STORE area.  Seeds NO `.be`, and
#       roots the scratch tree under /tmp whose ancestor chain up to `/`
#       carries no `.be` at all.  ($HOME is unusable here: a real
#       `$HOME/.be` sits above any $HOME-rooted dir and the walk would
#       find it.)  norepo tests only refuse / bootstrap-in-place; they
#       never mmap a pre-existing pack, so tmpfs is safe for them.

# rs_repo_base — echo (creating) the $HOME-rooted ext4 scratch base for
# REPO worktrees.  Honours a caller/cmake-supplied $TMP.
rs_repo_base() {
    printf '%s\n' "${TMP:-$HOME/tmp/be-tests-$(date +%Y%m%d-%H%M%S)}/${TEST_ID:-test}/$$"
}

# rs_norepo_base — echo (creating) a /tmp-rooted scratch base whose
# ancestor chain has no `.be`, for genuine no-store tests.
rs_norepo_base() {
    printf '%s\n' "${TMPDIR:-/tmp}/be-tests-norepo/${TEST_ID:-norepo}/$$"
}

# rs_fresh_wt [name] — create an isolated REPO worktree, cd into it, and
# seed the empty-`.be/` shield.  Wipes any leftover same-root state.
# Sets/exports $RS_ROOT (process scratch root) and $RS_WT.
rs_fresh_wt() {
    _rs_name=${1:-wt}
    : "${RS_ROOT:=$(rs_repo_base)}"
    RS_WT="$RS_ROOT/$_rs_name"
    rm -rf "$RS_WT"
    mkdir -p "$RS_WT/.be"
    cd "$RS_WT"
    export RS_ROOT RS_WT
}

# rs_shield <dir> — seed ONLY the empty-`.be/` repo shield at an
# explicit scratch dir (no cd).  The one place the shield is created;
# rs_wt_at / rs_fresh_wt build on it.  Use when the test cd's later.
rs_shield() {
    mkdir -p "$1/.be"
}

# rs_wt_at <dir> — seed the empty-`.be/` repo shield at an explicit
# scratch dir and cd into it.  For dog scripts that manage their own
# per-scenario dir names but must use the ONE shield procedure.  The
# dir must already live under an isolated base (rs_repo_base / $TMP).
rs_wt_at() {
    rs_shield "$1"
    cd "$1"
}

# rs_fresh_norepo [name] — create a genuine NO-STORE worktree: no `.be`
# here and none in any ancestor.  cd into it.  Sets/exports $RS_ROOT
# and $RS_WT.  Use for SNIFFnorepo-style refusal tests.
rs_fresh_norepo() {
    _rs_name=${1:-loose}
    : "${RS_NOREPO_ROOT:=$(rs_norepo_base)}"
    RS_WT="$RS_NOREPO_ROOT/$_rs_name"
    rm -rf "$RS_WT"
    mkdir -p "$RS_WT"
    cd "$RS_WT"
    export RS_NOREPO_ROOT RS_WT
}
