#!/bin/sh
#  setup-primitives.sh — common world-builders for verbcheck-driven
#  tests.  Each function expects to be called from inside a fresh wt
#  dir (use `vc_fresh_wt` from verbcheck.sh first), and leaves the wt
#  in a documented state.
#
#  These are toy-repo helpers — small, deterministic, controllable.
#  They are *not* meant to seed realistic histories.  Bigger tests
#  compose these.
#
#  Convention: each function exports the shas it produced as
#  uppercase shell vars (T1, T2, FEAT_TIP, …) so the test can refer
#  to them after the call.

# ----- helpers used internally ---------------------------------------

#  Latest sha recorded in .be/wtlog (most recent get/post/patch row).
sp_head_hex() {
    awk -F'\t' '$2=="post"||$2=="get"||$2=="patch" { last=$3 }
                END {
                    h = last; sub(/^[^#]*#/, "", h)
                    if (length(h) == 40 && h ~ /^[0-9a-f]+$/) print h
                }' .be/wtlog
}

#  Tip of a labelled branch via `keeper refs`.
sp_ref_tip() {
    keeper refs 2>/dev/null | awk -v k="$1" '
        { sub(/^[[:space:]]+/, "")
          tab = index($0, "\t")
          if (tab == 0) next
          kf = substr($0, 1, tab - 1)
          if (kf != k) next
          n = split($0, toks, /[[:space:]]+/)
          v = toks[n]; sub(/^\?/, "", v); print v; exit
        }'
}

# ----- world-builders ------------------------------------------------

#  Empty wt with a single `x.txt` committed on trunk.  Exports:
#    T1 — the commit sha.
sp_seed_trunk() {
    echo "x v1" > x.txt
    "$BE" post 'v1 msg' >/dev/null
    T1=$(sp_head_hex)
    [ -n "$T1" ] || { echo "sp_seed_trunk: no T1" >&2; exit 1; }
}

#  After sp_seed_trunk: append a second post on trunk (modify x.txt).
#  Exports:
#    T2 — second commit sha (T2 != T1; T1 is T2's parent).
sp_seed_two_tips() {
    sp_seed_trunk
    sleep 0.1
    echo "x v2" > x.txt
    "$BE" post 'v2 msg' >/dev/null
    T2=$(sp_head_hex)
    [ -n "$T2" ] && [ "$T2" != "$T1" ] \
        || { echo "sp_seed_two_tips: no T2 (got '$T2')" >&2; exit 1; }
}

#  After sp_seed_trunk: label `?feat` at T1.  Exports:
#    FEAT_TIP — sha that ?feat points at (= T1).
sp_label_feat() {
    "$BE" put '?feat' >/dev/null
    FEAT_TIP=$(sp_ref_tip "?feat")
    [ "$FEAT_TIP" = "$T1" ] \
        || { echo "sp_label_feat: ?feat at '$FEAT_TIP' != T1=$T1" >&2; exit 1; }
}

#  After sp_label_feat: switch wt onto ?feat.  No exports.
sp_switch_feat() {
    "$BE" get "?feat" >/dev/null
}

#  Make a tracked file's mtime fall outside the stamp-set so the wt
#  appears dirty.  Pass a path; defaults to x.txt.
sp_make_dirty() {
    p=${1:-x.txt}
    sleep 0.1
    echo "$(date +%N) dirty" >> "$p"
}

#  Resolve the canonical project shard's refs path.  Row 0 of
#  .be/wtlog (or .be if it's a regular FILE secondary wt) carries
#  `file:<abs>/.be/<project>/`; strip the `file:` prefix and the
#  trailing slash to get the shard dir.  Fallback to `.be/` for
#  legacy / unanchored worktrees.
sp_refs_path() {
    _wt=".be/wtlog"
    [ -f .be ] && [ ! -d .be ] && _wt=".be"
    #  Row 0 is the wt->store anchor: verb `get` (current) or `repo`
    #  (legacy stores), $3 = `file:<shard>/`.  Match by position.
    _anchor=$(awk -F'\t' 'NR==1 && ($2 == "get" || $2 == "repo") { print $3; exit }' "$_wt" 2>/dev/null)
    if [ -n "$_anchor" ]; then
        _shard=${_anchor#file:}
        _shard=${_shard%/}
        printf '%s/refs\n' "$_shard"
    else
        printf '.be/refs\n'
    fi
}

#  Write a present-but-unrelated tip into REFS for KEY.  Used by
#  non-ff tests — GRAFLca returns 0 for unknown shas, so the ff
#  guard fires.
sp_poison_refs() {
    key=$1                                    # e.g. "?" or "?feat"
    fake="deadbeefdeadbeefdeadbeefdeadbeefdeadbeef"
    _refs=$(sp_refs_path)
    ts=$(awk 'END { print $1 }' "$_refs")
    printf '%sz\tpost\t%s#%s\n' "$ts" "$key" "$fake" >> "$_refs"
}

#  Drop tracked files from disk (simulating a wiped wt).  Useful for
#  GET-restores-files scenarios.
sp_wipe_wt() {
    find . -type f \
        -not -path './.be' -not -path './.be/*' \
        -not -name '.be/wtlog' -not -name '.be/sniff.pid' \
        -delete 2>/dev/null || true
}
