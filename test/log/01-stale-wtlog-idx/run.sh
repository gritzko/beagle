#!/bin/sh
#  log/01-stale-wtlog-idx — DIS-033 regression.
#
#  A freshly-cloned / freshly-touched worktree can carry a STALE ULOG
#  sidecar (`.wtlog.idx` / `.refs.idx`) whose tail sentinel records the
#  current log's (size, mtime) but whose ROW ENTRIES describe only an
#  older, shorter PREFIX — a "false-fresh" index.  The coarse
#  (size, mtime) freshness check trusted such a sidecar verbatim, so the
#  tip rows appended after the stale prefix were dropped: `SNIFFAtTailOf`
#  missed the wt's current tip, no `--at` was forwarded, and `be log:`
#  returned an EMPTY listing (banner + exit 0 — looks like "no history")
#  while `be head` failed loud with GRAFFAIL "--at sha not set".  Only
#  the NEXT `be` invocation rebuilt the sidecar and the same `be log:`
#  then listed (a confusing transient).
#
#  This case forges the false-fresh `.wtlog.idx` deterministically (no
#  clone/wire dependence) and asserts:
#    (a) `be log:` LISTS every commit — never an empty exit-0 — i.e. the
#        stale sidecar is detected (rows don't span the log) and rebuilt;
#    (b) `be head` agrees: 0 ahead / 0 behind (cur == tip), exit 0 —
#        the two read paths no longer disagree on the no-`--at` cause.
#
#  HERMETIC: case.sh's rs_fresh_wt seeds the per-wt empty-`.be/` shield
#  + the scratch-base firewall, so store discovery can never escape to
#  the dev box's real $HOME/.be.

. "$(dirname "$0")/../../lib/case.sh"

# ---------------------------------------------------------------------
# 1. Seed a THREE-commit trunk.  Snapshot the wtlog sidecar after the
#    SECOND commit (a strictly-shorter prefix of the final log).
# ---------------------------------------------------------------------
printf 'a\n' > a.txt
"$BE" put a.txt   >/dev/null 2>seed.err || { cat seed.err >&2; echo "FAIL: put a" >&2; exit 1; }
"$BE" post '#C1'  >/dev/null 2>seed.err || { cat seed.err >&2; echo "FAIL: post C1" >&2; exit 1; }

printf 'b\n' > b.txt
"$BE" put b.txt   >/dev/null 2>seed.err || { cat seed.err >&2; echo "FAIL: put b" >&2; exit 1; }
"$BE" post '#C2'  >/dev/null 2>seed.err || { cat seed.err >&2; echo "FAIL: post C2" >&2; exit 1; }

[ -f .be/wtlog ] && [ -f .be/.wtlog.idx ] \
    || { echo "FAIL(setup): .be/wtlog + .be/.wtlog.idx expected" >&2; ls -la .be >&2; exit 1; }
cp .be/.wtlog.idx prefix.idx          # 2-commit-state sidecar (short prefix)

printf 'c\n' > c.txt
"$BE" put c.txt   >/dev/null 2>seed.err || { cat seed.err >&2; echo "FAIL: put c" >&2; exit 1; }
"$BE" post '#C3'  >/dev/null 2>seed.err || { cat seed.err >&2; echo "FAIL: post C3" >&2; exit 1; }

# Sanity: a healthy log lists all three before we corrupt the sidecar.
"$BE" log: >pre.out 2>pre.err || { cat pre.err >&2; echo "FAIL: healthy log:" >&2; exit 1; }
for m in C1 C2 C3; do
    grep -q "$m" pre.out || { echo "FAIL(setup): healthy log: missing $m" >&2; cat pre.out >&2; exit 1; }
done

# ---------------------------------------------------------------------
# 2. FORGE the false-fresh sidecar: the short (2-commit) PREFIX row
#    entries, but with the CURRENT (3-commit) log's tail sentinel —
#    so (size, mtime) match the live log while the rows stop short.
#    Each ULOG index entry is 16 bytes; the LAST entry is the sentinel.
# ---------------------------------------------------------------------
python3 - <<'PY' || { echo "FAIL: could not forge false-fresh sidecar" >&2; exit 1; }
pre  = open("prefix.idx", "rb").read()        # short prefix + its own sentinel
cur  = open(".be/.wtlog.idx", "rb").read()    # full log + correct sentinel
assert len(pre) >= 32 and len(cur) >= 16, (len(pre), len(cur))
sent = cur[-16:]                              # full-log sentinel (size+mtime)
rows = pre[:-16]                              # drop the prefix's own sentinel
open(".be/.wtlog.idx", "wb").write(rows + sent)
PY

# ---------------------------------------------------------------------
# 3a. `be log:` MUST rebuild the stale sidecar and list all commits —
#     never a silent empty exit-0.
# ---------------------------------------------------------------------
"$BE" log: >log.out 2>log.err
rc=$?
[ "$rc" = 0 ] || { echo "FAIL: be log: exited $rc" >&2; cat log.err >&2; exit 1; }
[ -s log.out ] || { echo "FAIL: be log: returned EMPTY (DIS-033 silent-empty)" >&2; exit 1; }
for m in C1 C2 C3; do
    grep -q "$m" log.out || { echo "FAIL: be log: dropped $m (stale prefix not rebuilt)" >&2; cat log.out >&2; exit 1; }
done

# ---------------------------------------------------------------------
# 3b. `be head` agrees — cur == tip, 0 ahead / 0 behind, exit 0.
#     (Before the fix this read path failed loud with GRAFFAIL while
#     `be log:` failed silent — the two no longer disagree.)
# ---------------------------------------------------------------------
"$BE" head >head.out 2>head.err
rc=$?
[ "$rc" = 0 ] || { echo "FAIL: be head exited $rc" >&2; cat head.err >&2; exit 1; }
grep -q '0 ahead, 0 behind' head.out \
    || { echo "FAIL: be head not up-to-date" >&2; cat head.out >&2; exit 1; }

echo "log/01-stale-wtlog-idx: OK (stale false-fresh sidecar rebuilt; log: + head agree)"
