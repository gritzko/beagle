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
#
# --- HERMETIC FIREWALL (DIS-024 stage 0) -------------------------------
#   Belt-and-suspenders to the per-wt shield: every REPO-mode entry
#   drops an EMPTY `.be` FILE at the scratch base (`$HOME/tmp`, the
#   parent of every per-run dir).  An empty `.be` file is an INVALID
#   secondary-wt anchor — `home_walk_up` hitting it returns NOTAWT
#   (dog/HOME.c::home_anchor_resolve), covered by HOMEtest's
#   "secondary empty .be file" case.  So if a wt shield ever breaks
#   (e.g. a stray shard subdir flips home_dir_no_subdirs), the walk is
#   stopped at the base instead of ascending into the dev box's real
#   `$HOME/.be` — silent real-store corruption becomes a contained,
#   loud test failure.  The file sits strictly below `$HOME` (so
#   `$HOME/.be` is unreachable) yet above all test scratch (so a
#   wt's own `.be/` shield is always found first).  No env var, no
#   discovery-code change: it rides the existing empty-anchor refusal.

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

# rs_firewall — drop an empty `.be` FILE at the REPO scratch base (the
# parent of the per-run dir, i.e. $HOME/tmp by default).  A walk that
# escapes a broken wt shield stops there with NOTAWT instead of reaching
# the dev box's real `$HOME/.be`.  Idempotent; refuses to touch `$HOME`
# or `/` themselves so it can never clobber the real store.
rs_firewall() {
    _rs_fw_base=$(dirname "${TMP:-$HOME/tmp/_}")
    [ -n "$_rs_fw_base" ] && [ "$_rs_fw_base" != "$HOME" ] \
        && [ "$_rs_fw_base" != "/" ] || return 0
    mkdir -p "$_rs_fw_base" 2>/dev/null || return 0
    [ -e "$_rs_fw_base/.be" ] || : > "$_rs_fw_base/.be"
}

# rs_fresh_wt [name] — create an isolated REPO worktree, cd into it, and
# seed the empty-`.be/` shield.  Wipes any leftover same-root state.
# Sets/exports $RS_ROOT (process scratch root) and $RS_WT.
rs_fresh_wt() {
    _rs_name=${1:-wt}
    : "${RS_ROOT:=$(rs_repo_base)}"
    rs_firewall
    RS_WT="$RS_ROOT/$_rs_name"
    rm -rf "$RS_WT"
    mkdir -p "$RS_WT/.be"
    cd "$RS_WT"
    #  The first rw command bootstraps a born-sharded store; the project
    #  (Title) defaults to the wt basename, so its `refs`/packs live in
    #  `.be/<name>/`.  Export RS_SHARD (relative to the wt root, where
    #  tests run) so assertions read `$RS_SHARD/refs`, not flat `.be/refs`.
    RS_SHARD=".be/$_rs_name"
    export RS_ROOT RS_WT RS_SHARD
}

# rs_shield <dir> — seed ONLY the empty-`.be/` repo shield at an
# explicit scratch dir (no cd).  The one place the shield is created;
# rs_wt_at / rs_fresh_wt build on it.  Use when the test cd's later.
rs_shield() {
    rs_firewall
    mkdir -p "$1/.be"
    #  Shard path the bootstrap will mint (project = wt basename); see
    #  rs_fresh_wt.  Path only — no dir created here.
    RS_SHARD=".be/$(basename "$1")"
    export RS_SHARD
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
