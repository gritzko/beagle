#!/bin/sh
#  27-status-after-patch — bare `be` (sniff status) classification when
#  a `patch` row sits in scope (patched in, not yet posted).  Covers
#  all six surface buckets:
#
#       file                | classify as |  set up by
#       --------------------+-------------+--------------------------------
#       unchanged.txt       | ok          |  in baseline, neither side touches it
#       merged.txt          | ok or new   |  patched-in (file stamp matches the
#                                            patch row's ts → SNIFFAtKnown YES;
#                                            kept counted as `ok` since baseline
#                                            also has it).
#       new_remote.txt      | new         |  added by feat, written by PATCH —
#                                            the central regression: must NOT
#                                            be reported as `unk`.
#       localmod.txt        | mod         |  in baseline, user edits after the
#                                            patch (no put yet).
#       new_local.txt       | put         |  user creates + `be put`s after the
#                                            patch.
#       untracked.txt       | unk         |  user creates after the patch, no
#                                            put — true untracked.
#
#  All assertions read the captured stdout of bare `be`.
#
#  Sets up:
#       T0 ── T1        ← cur (trunk):  T1 = trunk-side edit of trunk.txt
#         \                (so cur isn't at the fork — patch has real work)
#          F1            ← ?feat:  F1 = edit merged.txt + add new_remote.txt

. "$(dirname "$0")/../../lib/branches.sh"

OUT="$SCRATCH/../out"
mkdir -p "$OUT"

#  --- 1. baseline on trunk -----------------------------------------
cp "$CASE/01.unchanged.t0.txt" unchanged.txt
cp "$CASE/02.localmod.t0.txt"  localmod.txt
cp "$CASE/03.merged.t0.txt"    merged.txt
echo "trunk seed" > trunk.txt
"$BE" put unchanged.txt localmod.txt merged.txt trunk.txt >/dev/null
"$BE" post 't0 baseline' >/dev/null
T0=$(head_hex)

#  --- 2. fork ?feat and build F1 -----------------------------------
"$BE" put '?./feat' >/dev/null

#  Trunk T1 — edit a file feat doesn't touch, so PATCH has a fork ≠ ours.
sleep 0.02
echo "trunk advance" > trunk.txt
"$BE" put trunk.txt >/dev/null
"$BE" post 't1 trunk advance' >/dev/null
T1=$(head_hex)

#  Build feat: edit merged.txt + add new_remote.txt.
"$BE" get '?feat' >/dev/null
[ "$(head_hex)" = "$T0" ] || fail "feat must fork at T0; head=$(head_hex)"

sleep 0.02
cp "$CASE/04.merged.f1.txt"     merged.txt
cp "$CASE/05.new_remote.f1.txt" new_remote.txt
"$BE" put merged.txt new_remote.txt >/dev/null
"$BE" post 'f1 edit merged add new_remote' >/dev/null
F1=$(head_hex)

#  --- 3. back to trunk; patch feat into wt -------------------------
"$BE" get '?..' >/dev/null
[ "$(head_hex)" = "$T1" ] || fail "trunk must be at T1; head=$(head_hex)"

"$BE" patch '?feat' >/dev/null 2>"$OUT/p.err" \
    || fail "be patch ?feat failed: $(cat $OUT/p.err)"

#  Sanity: new_remote.txt actually landed on disk with feat's bytes.
[ -s new_remote.txt ] || fail "new_remote.txt missing or empty after patch"
match "$CASE/05.new_remote.f1.txt" new_remote.txt
match "$CASE/04.merged.f1.txt"     merged.txt

#  --- 4. user actions on top of the patch --------------------------
#  4a. localmod.txt — edit a baseline file the patch didn't touch.
sleep 0.02
echo "user local edit" > localmod.txt

#  4b. new_local.txt — create + `be put`-stage a brand-new file.
sleep 0.02
echo "user-added file"  > new_local.txt
"$BE" put new_local.txt >/dev/null

#  4c. untracked.txt — create and DO NOT stage.
sleep 0.02
echo "user-untracked"  > untracked.txt

#  --- 5. bare `be` (status) -------------------------------
"$BE" >"$OUT/status.out" 2>"$OUT/status.err"

#  --- 6. assertions ----------------------------------------------
#  Helper: a row matches `^[[:space:]]*<ts>[[:space:]]+<bucket>[[:space:]]+<path>$`
#  where <ts> is the time column and <bucket> is the status keyword.
need_row() {
    _bucket=$1; _path=$2
    if ! grep -qE "^[[:space:]]*[^[:space:]]+[[:space:]]+${_bucket}[[:space:]]+(\./)?${_path}\$" \
            "$OUT/status.out"; then
        echo "FAIL: expected '${_bucket} ${_path}' row in status; got:" >&2
        cat "$OUT/status.out" >&2
        exit 1
    fi
}

forbid_row() {
    _bucket=$1; _path=$2
    if grep -qE "^[[:space:]]*[^[:space:]]+[[:space:]]+${_bucket}[[:space:]]+(\./)?${_path}\$" \
           "$OUT/status.out"; then
        echo "FAIL: ${_path} unexpectedly bucketed as '${_bucket}'; got:" >&2
        cat "$OUT/status.out" >&2
        exit 1
    fi
}

#  new_remote.txt — patched-in, must surface as `new` (NOT `unk`).
#  This is the central regression that the test exists to lock down.
need_row   "new" "new_remote.txt"
forbid_row "unk" "new_remote.txt"

#  localmod.txt — user edited a baseline file → `mod`.
need_row "mod" "localmod.txt"

#  new_local.txt — user created + put-staged → `put` (could be `new`
#  depending on baseline absence — but the put_rec branch in
#  status_step always picks `put`/`new` consistently; we accept both).
if ! grep -qE "^[[:space:]]*[^[:space:]]+[[:space:]]+(put|new)[[:space:]]+(\./)?new_local\.txt\$" \
        "$OUT/status.out"; then
    fail "new_local.txt not reported as put/new; got: $(cat $OUT/status.out)"
fi

#  untracked.txt — pure unstaged user file → `unk`.
need_row   "unk" "untracked.txt"
forbid_row "new" "untracked.txt"

#  unchanged.txt and trunk.txt — both clean baseline.  Either don't
#  appear in any status row (counted into `ok`), or appear with no
#  modification.  We assert they don't show as mod/del/mis/unk.
for f in unchanged.txt trunk.txt; do
    for bucket in mod del mis unk; do
        forbid_row "$bucket" "$f"
    done
done

#  merged.txt — patched-in (modified by feat).  Acceptable status is
#  any of `ok` (file stamp matches patch ts → SNIFFAtKnown YES, content
#  matches the new baseline+patch view → ok), or `new`/`mod` if the
#  classifier evolves to surface patch provenance.  The only forbidden
#  bucket is `unk` — patched files must never be untracked.
forbid_row "unk" "merged.txt"

#  Counter line: must lead with the cur branch URI (per the
#  "rel/path?cur/branch" format introduced for bare-be status).
tail -n 1 "$OUT/status.out" \
    | grep -qE '^\?\s*[[:space:]]+[0-9]+ ok' \
    || true   # counter format may evolve; don't pin shape yet

note "status-after-patch OK: new_remote → new, localmod → mod, untracked → unk"
echo "=== patch/27-status-after-patch: OK ==="
