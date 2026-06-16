#!/bin/sh
#  fragref.sh — GET-023: `be get '?#<sha>'` honours the fragment, and a
#  MALFORMED fragment is refused loudly instead of silently landing on
#  the wrong commit.
#
#  The bug: a present-but-empty query (`?`) with a `#fragment` had the
#  fragment SILENTLY DROPPED — REFSResolve only ever reads the QUERY, so
#  the empty `?` matched the trunk row and the get always landed on TRUNK
#  no matter what `<sha>` was asked for.  Worse, a trailing-dots fragment
#  (`?#<sha>...`) — meant to check out `<sha>` — `whiff_hex_hashlet`
#  ignored the non-hex tail and the prefix lookup mis-aligned, resolving
#  to the WRONG commit (observed: the target's parent) with exit 0.
#
#  Fix (sniff/GET.c::SNIFFGetURI): an empty-query `#fragment` IS the
#  detached checkout target — classify it as (valid full sha | valid
#  hashlet); resolve to it, else REFUSE.  Never fall through to trunk.
#
#  Table-driven: the wt records its cur tip in `.be/wtlog` as
#  `get\t?<40hex>` (detached) / `get\t?<branch>#<sha>`; we read back the
#  pinned sha to prove WHICH commit each form landed on.
#
#  Run: BIN=build-debug/bin sh sniff/test/fragref.sh
set -eu

BIN=${BIN:-$(dirname "$(command -v be)")}
export PATH="$BIN:$PATH"
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"

TEST_ID=${TEST_ID:-SNIFFfragref}
. "$(dirname "$0")/../../test/lib/repo-setup.sh"
TMP=$(rs_repo_base)
trap '_rc=$?; [ "$_rc" -eq 0 ] && { rm -rf "$TMP"; rmdir "${TMP%/*}" 2>/dev/null || true; rmdir "${TMP%/*/*}" 2>/dev/null || true; }' EXIT INT TERM
mkdir -p "$TMP"

fail() { echo "FAIL: $*" >&2; exit 1; }
note() { echo "  - $*"; }

#  The hex sha/hashlet the wt currently records as its cur tip — the
#  latest get/post/patch row in `.be/wtlog`.  Row shapes:
#    post           ?#<40hex>
#    get (full sha) ?<40hex>
#    get (hashlet)  ?<hashlet>#<hashlet>
#    get (branch)   ?<branch>#<40hex>
#  We want the resolved object id: take the LAST hex run on the URI
#  field (the fragment after `#` when present, else the query after `?`).
cur_pin() {
    awk -F'\t' '$2=="post" || $2=="get" || $2=="patch" { last=$3 }
                END {
                    s = last
                    if (index(s, "#") > 0) sub(/^.*#/, "", s)  # fragment
                    else sub(/^.*\?/, "", s)                   # query
                    print s
                }' .be/wtlog
}

WT="$TMP/wt"
rs_wt_at "$WT"

# --- build a 3-commit trunk history -----------------------------------
echo "=== setup: c1 -> c2 -> c3 on trunk ==="
echo "v1" > f.txt; be post -m c1 >/dev/null
echo "v2" > f.txt; be post -m c2 >/dev/null
echo "v3" > f.txt; be post -m c3 >/dev/null

#  Full shas of each commit (post rows, in order) from the project shard.
C1=$(sed -n '2p' .be/wtlog | grep -oE '[0-9a-f]{40}')
C2=$(sed -n '3p' .be/wtlog | grep -oE '[0-9a-f]{40}')
C3=$(sed -n '4p' .be/wtlog | grep -oE '[0-9a-f]{40}')
[ -n "$C1" ] && [ -n "$C2" ] && [ -n "$C3" ] \
    || fail "could not read commit shas from .be/wtlog"
H1=$(printf '%s' "$C1" | cut -c1-8)     # 8-char hashlet of c1
note "C1=$C1 C2=$C2 C3=$C3 (tip=C3)"

# --- VALID: bare ? (trunk) resolves to the trunk tip (no regression) --
#  Run while the wt is still on trunk's tip (right after setup) — the
#  bare-? trunk lookup must keep working alongside the new fragment arm.
echo "=== 0. ?  (bare trunk) resolves to C3 ==="
be get "?" >/dev/null 2>&1 || fail "bare ? (trunk) should resolve"
[ "$(cur_pin)" = "$C3" ] \
    || fail "bare ? landed on $(cur_pin), want trunk tip C3"
note "bare ? (trunk) correctly pinned C3"

# =====================================================================
#  Table of cases.  Each: a fragment FORM, whether it must RESOLVE (and
#  to which sha) or be REFUSED.  Driven against the SAME 3-commit repo.
# =====================================================================

# --- VALID: full-sha fragment of an ancestor pins THAT sha (not trunk) -
echo "=== 1. ?#<full C1>  -> pins C1 (was: silently trunk/C3) ==="
be get "?#$C1" >/dev/null 2>&1 || fail "?#<full-sha> should resolve"
[ "$(cur_pin)" = "$C1" ] \
    || fail "?#<full C1> landed on $(cur_pin), want $C1 (silent trunk fallthrough)"
note "?#<full C1> correctly pinned C1"

# --- VALID: 8-char hashlet fragment pins the same commit --------------
#  (GETCheckout records the hashlet verbatim in the wtlog, so compare
#  against the hashlet prefix — it must be C1's prefix, not C3's.)
echo "=== 2. ?#<hashlet C1>  -> pins C1's hashlet ==="
be get "?#$C3" >/dev/null 2>&1 || fail "reset to C3 failed"
be get "?#$H1" >/dev/null 2>&1 || fail "?#<hashlet> should resolve"
[ "$(cur_pin)" = "$H1" ] \
    || fail "?#<hashlet C1> landed on $(cur_pin), want $H1"
note "?#<hashlet C1> correctly pinned C1 ($H1)"

# --- VALID: full-sha in the QUERY (canonical detached form) -----------
echo "=== 3. ?<full C2>  (sha in query) -> pins C2 ==="
be get "?$C2" >/dev/null 2>&1 || fail "?<full-sha> query form should resolve"
[ "$(cur_pin)" = "$C2" ] \
    || fail "?<full C2> landed on $(cur_pin), want $C2"
note "?<full C2> query form correctly pinned C2"

# --- INVALID: trailing dots must be REFUSED, never silent-resolve -----
#  These are the GET-023 regression cases.  Reset to C3 first so a
#  silent-fallthrough bug would leave a DIFFERENT (wrong) pin behind.
for SUFFIX in '.' '..' '...'; do
    echo "=== 4. ?#<full C1>$SUFFIX  -> MUST refuse ==="
    be get "?#$C3" >/dev/null 2>&1 || fail "reset to C3 failed"
    if be get "?#$C1$SUFFIX" >/dev/null 2>&1; then
        fail "?#<sha>$SUFFIX resolved silently (exit 0) — must refuse"
    fi
    #  A refused get must NOT move the wt off C3.
    [ "$(cur_pin)" = "$C3" ] \
        || fail "refused ?#<sha>$SUFFIX moved the wt to $(cur_pin) (want C3)"
    note "?#<sha>$SUFFIX refused; wt unmoved"
done

# --- INVALID: a random non-hex non-branch word fragment is refused ----
echo "=== 5. ?#zzzzzzzz  (non-hex, no branch) -> MUST refuse ==="
be get "?#$C3" >/dev/null 2>&1 || fail "reset to C3 failed"
if be get "?#zzzzzzzz" >/dev/null 2>&1; then
    fail "?#<non-hex> resolved silently — must refuse"
fi
[ "$(cur_pin)" = "$C3" ] \
    || fail "refused ?#<non-hex> moved the wt to $(cur_pin) (want C3)"
note "?#<non-hex> refused; wt unmoved"

# =====================================================================
#  SHARED RESOLVER (graf/LOG.c::GRAFResolveTip) — the SAME classify-or-
#  reject binary used by log:/blame:/diff:.  A fragment is either a valid
#  hex object-id (full sha / hashlet) or it is NOT — there is NO "spoilt
#  hex" middle state.  An empty-query `#<sha>...` (trailing dots) /
#  `#<word>` is NOT a valid id, so it must MISS CLEANLY (resolver errors,
#  non-zero exit) — it must NEVER silently fall through to the trunk tip
#  (which would render the WHOLE trunk log as if a valid ref was given).
#  Only a `#<N>` count is a legitimate empty-query non-object fragment.
#
#  Reach the resolver directly via `graf` (be drops the fragment before
#  spawning the projector, so we invoke graf with the same `--at` be
#  would forward).  Skip gracefully if graf isn't on PATH.
# =====================================================================
if command -v graf >/dev/null 2>&1; then
    AT="$WT?/wt#$C3"           # same --at shape be forwards to graf
    echo "=== 6. log:?#<full C1>  -> resolves to C1 (one commit) ==="
    LOG1=$(graf --at "$AT" --plain "log:?#$C1" 2>/dev/null) \
        || fail "log:?#<full C1> errored — should resolve to C1"
    printf '%s\n' "$LOG1" | grep -q "$H1" \
        || fail "log:?#<full C1> did not scope to C1 ($H1): [$LOG1]"
    note "log:?#<full C1> resolved to C1"

    echo "=== 7. log:?#<full C1>...  -> MUST miss cleanly (no trunk log) ==="
    if graf --at "$AT" --plain "log:?#$C1..." >/dev/null 2>&1; then
        fail "log:?#<sha>... resolved silently (exit 0) — must refuse, \
not render trunk"
    fi
    note "log:?#<sha>... refused (non-zero); no silent trunk resolve"

    echo "=== 8. log:?#zzzzzzzz  -> MUST miss cleanly ==="
    if graf --at "$AT" --plain "log:?#zzzzzzzz" >/dev/null 2>&1; then
        fail "log:?#<non-hex> resolved silently — must refuse"
    fi
    note "log:?#<non-hex> refused; no silent trunk resolve"

    echo "=== 9. log:?#3  -> count still works (resolves trunk, caps walk) ==="
    LOG3=$(graf --at "$AT" --plain "log:?#3" 2>/dev/null) \
        || fail "log:?#<N> count errored — count must keep working"
    [ "$(printf '%s\n' "$LOG3" | grep -c '(')" -ge 1 ] \
        || fail "log:?#<N> count rendered no commits: [$LOG3]"
    note "log:?#<N> count still resolves trunk + caps the walk"
else
    note "graf not on PATH — skipped shared-resolver (log:) cases"
fi

echo "=== all fragref scenarios passed ==="
