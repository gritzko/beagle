#!/bin/sh
#  head/23-trunk-no-remote — HEAD-002 regression.
#
#  Two facets, both deterministic in a hermetic single-project store
#  with NO remote-tracking ref:
#
#  (a) Resolution: bare `be head` (and `be head ?`, `be head ?..`) on a
#      self-hosted trunk must resolve the trunk's OWN tip and report
#      "up to date" (0 ahead, 0 behind, 0 changed) — NOT fail with
#      `graf: head: cannot resolve target` / GRAFNONE / exit 206.  The
#      trunk tip IS resolvable from the local `?` REFS row; the absence
#      of a cached remote counterpart is "up to date", not an error
#      (https://replicated.wiki/html/wiki/HEAD.html §HEAD).
#
#  (b) Reporting coherence: a genuinely-unresolvable leg (`#nomatch`
#      message search) must exit non-zero with an `Error:` line that
#      AGREES with graf's own diagnostic — `Error: GRAFNONE`, never a
#      contradictory generic `Error: NONE` over a GRAFNONE leg.

. "$(dirname "$0")/../../lib/case.sh"

cd "$SCRATCH"

# Anchor the project shard so the name isn't derived from a URI basename.
"$BE" put "?/$P/" 2>/dev/null || true

# Three commits on trunk — a resolvable parent tip, no remote anywhere.
echo a > a.txt;  "$BE" put a.txt >/dev/null;  "$BE" post '#T1' >/dev/null
echo b > b.txt;  "$BE" put b.txt >/dev/null;  "$BE" post '#T2' >/dev/null
echo c > c.txt;  "$BE" put c.txt >/dev/null;  "$BE" post '#T3' >/dev/null

# --- (a) bare `be head` resolves trunk tip → 0/0, exit 0, clean err ---
"$BE" head >01.bare.got.out 2>01.bare.got.err
rc=$?
[ "$rc" = 0 ] || {
    echo "bare be head exited $rc (want 0); stderr:" >&2
    cat 01.bare.got.err >&2
    exit 1
}
empty 01.bare.got.err
match_re "$CASE/01.bare.want.txt" 01.bare.got.out

# --- (a) explicit `be head ?` (absolute trunk) — same up-to-date ------
"$BE" head '?' >02.qmark.got.out 2>02.qmark.got.err
rc=$?
[ "$rc" = 0 ] || { echo "be head ? exited $rc" >&2; cat 02.qmark.got.err >&2; exit 1; }
empty 02.qmark.got.err
match_re "$CASE/02.qmark.want.txt" 02.qmark.got.out

# --- (b) genuinely-unresolvable leg: consistent non-zero + GRAFNONE ---
#  `be head '#zzz-no-match'` must exit non-zero AND its be-level Error
#  line must read GRAFNONE (matching graf's), never a generic NONE.
if "$BE" head '#zzz-no-match' >03.miss.got.out 2>03.miss.got.err; then
    echo "head: expected non-zero exit on unresolvable leg" >&2
    exit 1
fi
empty 03.miss.got.out
match_re "$CASE/03.miss.err.txt" 03.miss.got.err
# The trailing be-level Error must NOT contradict graf's leg diagnostic.
if grep -q '^Error: NONE$' 03.miss.got.err; then
    echo "head: contradictory generic 'Error: NONE' over a GRAFNONE leg" >&2
    cat 03.miss.got.err >&2
    exit 1
fi

echo "head/23-trunk-no-remote: OK (trunk resolves 0/0; unresolvable leg = GRAFNONE non-zero)"
