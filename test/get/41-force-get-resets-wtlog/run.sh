#!/bin/sh
#  get/41-force-get-resets-wtlog — GET-008.
#
#  `be get!` (≡ `be get --force`) is the clean-slate reset.  It does NOT
#  truncate the append-only `.be/wtlog`; instead its freshly-appended
#  `get` row IS a boundary that voids every prior staged `put`/`patch`
#  record (AT.c: a `get` resets both the pd boundary, SNIFFAtLastPostTs,
#  and the patch boundary, at_patch_boundary_start).  So a poisoned
#  wtlog — stale staged rows that would otherwise make the next `be post`
#  commit unwanted content (or fail with POSTNOMSG) — is cleared simply
#  by running `be get!` to re-checkout the CURRENT tip (a no-op overlay).
#
#  The crux this case pins: a no-op force re-checkout (`be get! '?'`,
#  target == cur) STILL appends its `get` boundary row.  GETCheckout has
#  no "target == baseline → return OK" short-circuit; it always reaches
#  the COMMIT POINT (sniff/GET.c SNIFFAtAppendAt).  If that ever
#  regressed to skip the append on a no-op, the reset would silently
#  stop working and this case would fail.
#
#  HERMETIC: case.sh's rs_fresh_wt seeds the per-wt empty-`.be/` shield
#  + the scratch-base firewall, so discovery can never escape to the dev
#  box's real $HOME/.be.

. "$(dirname "$0")/../../lib/case.sh"

#  wtlog_get_rows FILE — count `get` rows in a wtlog (tab-delimited
#  `<ts>\tget\t<uri>`).  Used to assert a fresh boundary was appended.
wtlog_get_rows() {
    #  Match the tab-anchored `get` column directly on the binary file:
    #  `grep -ao` treats binary as text and emits one match per line so
    #  `wc -l` counts occurrences.  Portable to BSD grep (macOS):
    #  using `strings` first would drop the surrounding tabs on macOS's
    #  BSD strings(1) and the count would always read 0.
    _wgr_tab=$(printf '\t')
    grep -aoE "${_wgr_tab}get${_wgr_tab}" "$1" 2>/dev/null | wc -l | tr -d ' '
}

# ---------------------------------------------------------------------
# Stash shell-redirected stdout/stderr OUTSIDE the wt.  `be get!` is a
# clean-slate force reset (GET-016) that may unlink wt-only files whose
# mtime ties them to a wtlog row (force_orphan_cb's SNIFFAtKnown gate).
# A coarse-resolution filesystem (1-second mtime granularity, seen on
# some macOS CI runners) can give shell-created `*.out`/`*.err` files
# an mtime that collides with a wtlog row's ts (ron60 is ms-resolution),
# making them look "tracked" — `3.get.err` then vanishes mid-test.
# Logs live in $LOG_DIR, a sibling of the wt, so sniff never scans them.
# ---------------------------------------------------------------------
LOG_DIR="$(dirname "$SCRATCH")/${NAME}-io"
mkdir -p "$LOG_DIR"

# ---------------------------------------------------------------------
# 1. Seed a one-commit store.
# ---------------------------------------------------------------------
printf 'hello\n' > a.txt
"$BE" put a.txt   > "$LOG_DIR/1.put.out"  2> "$LOG_DIR/1.put.err"  || { cat "$LOG_DIR/1.put.err"  >&2; echo "FAIL: put a.txt" >&2; exit 1; }
"$BE" post '#seed' > "$LOG_DIR/1.post.out" 2> "$LOG_DIR/1.post.err" || { cat "$LOG_DIR/1.post.err" >&2; echo "FAIL: post seed" >&2; exit 1; }

[ -f .be/wtlog ] || { echo "FAIL(setup): .be/wtlog missing" >&2; exit 1; }
GETS0=$(wtlog_get_rows .be/wtlog)
[ "$GETS0" -ge 1 ] || { echo "FAIL(setup): expected a bootstrap get row, found $GETS0" >&2; exit 1; }

# ---------------------------------------------------------------------
# 2. POISON: stage a `put` for a NEW file AFTER the post.  Without the
#    reset, the next `be post` would commit b.txt (the symptom GET-008
#    targets — a stale stage leaking into the next commit).
# ---------------------------------------------------------------------
printf 'world\n' > b.txt
"$BE" put b.txt > "$LOG_DIR/2.put.out" 2> "$LOG_DIR/2.put.err" || { cat "$LOG_DIR/2.put.err" >&2; echo "FAIL: put b.txt" >&2; exit 1; }

cp .be/wtlog "$LOG_DIR/2.wtlog.before"
GETS_BEFORE=$(wtlog_get_rows .be/wtlog)

# ---------------------------------------------------------------------
# 3. `be get! '?'` — force re-checkout to the CURRENT tip (a no-op
#    overlay).  MUST append a fresh `get` boundary row even though the
#    target == baseline.  No rows are ever deleted (append-only).
# ---------------------------------------------------------------------
"$BE" get! '?' > "$LOG_DIR/3.get.out" 2> "$LOG_DIR/3.get.err" || { cat "$LOG_DIR/3.get.err" >&2; echo "FAIL: be get! '?'" >&2; exit 1; }
#  GET-026: the force re-checkout reports its resulting state as a
#  `get ?#<hashlet>` banner on stdout (the boundary-append proof is the
#  wtlog grow check below).
grep -qE 'get [?]#[0-9a-f]{8}' "$LOG_DIR/3.get.out" || {
    echo "FAIL: be get! '?' did not report the state banner" >&2
    cat "$LOG_DIR/3.get.out" "$LOG_DIR/3.get.err" >&2; exit 1
}

GETS_AFTER=$(wtlog_get_rows .be/wtlog)
if [ "$GETS_AFTER" -le "$GETS_BEFORE" ]; then
    echo "FAIL: no-op force get appended NO boundary row" >&2
    echo "  get-rows before=$GETS_BEFORE after=$GETS_AFTER" >&2
    echo "  --- wtlog before ---" >&2; strings "$LOG_DIR/2.wtlog.before" >&2
    echo "  --- wtlog after  ---" >&2; strings .be/wtlog >&2
    exit 1
fi

#  Append-only: the pre-reset prefix is preserved byte-for-byte; the
#  reset only GREW the log (no truncation / row deletion).
SZ_BEFORE=$(wc -c < "$LOG_DIR/2.wtlog.before")
SZ_AFTER=$(wc -c < .be/wtlog)
[ "$SZ_AFTER" -gt "$SZ_BEFORE" ] || {
    echo "FAIL: wtlog did not grow (expected append; size $SZ_BEFORE -> $SZ_AFTER)" >&2
    exit 1
}
head -c "$SZ_BEFORE" .be/wtlog > "$LOG_DIR/3.wtlog.prefix"
if ! cmp -s "$LOG_DIR/2.wtlog.before" "$LOG_DIR/3.wtlog.prefix"; then
    echo "FAIL: prior wtlog bytes were rewritten (NOT append-only)" >&2
    cmp "$LOG_DIR/2.wtlog.before" "$LOG_DIR/3.wtlog.prefix" >&2 || true
    exit 1
fi

# ---------------------------------------------------------------------
# 4. The staged `put b.txt` is now BELOW the get boundary -> `be post`
#    is a CLEAN no-op (POSTNONE: no changes since base), NOT a commit of
#    b.txt.  Exit is non-zero (POSTNONE) — assert the message, not 0.
# ---------------------------------------------------------------------
if "$BE" post 'should be a no-op' > "$LOG_DIR/4.post.out" 2> "$LOG_DIR/4.post.err"; then
    echo "FAIL: be post committed after reset (staged b.txt leaked past boundary)" >&2
    cat "$LOG_DIR/4.post.err" >&2
    exit 1
fi
grep -qE 'POSTNONE' "$LOG_DIR/4.post.err" || {
    echo "FAIL: expected POSTNONE (clean no-op) after reset; got:" >&2
    cat "$LOG_DIR/4.post.err" >&2
    exit 1
}

# ---------------------------------------------------------------------
# 5. b.txt was NOT committed; the log still lists exactly the seed (idx
#    intact after the append — DIS-033: put/patch rows never touch
#    refs/.refs.idx, so `be log:` keeps working).
# ---------------------------------------------------------------------
"$BE" log: > "$LOG_DIR/5.log.out" 2> "$LOG_DIR/5.log.err" || { cat "$LOG_DIR/5.log.err" >&2; echo "FAIL: be log: errored after reset" >&2; exit 1; }
grep -q 'seed' "$LOG_DIR/5.log.out" || { echo "FAIL: log: lost the seed commit" >&2; cat "$LOG_DIR/5.log.out" >&2; exit 1; }
if grep -q 'should be a no-op' "$LOG_DIR/5.log.out"; then
    echo "FAIL: a spurious post landed in the log" >&2; cat "$LOG_DIR/5.log.out" >&2; exit 1
fi

echo "OK: be get! '?' appended a get boundary (append-only); staged put voided; post clean no-op; log intact"
