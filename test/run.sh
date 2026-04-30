#!/bin/sh
# test/run.sh — top-level suite runner.  Iterates every verb dir
# (subdirs of test/ with a lowercase name; lib/ is skipped) and
# invokes its run.sh.  Aggregate pass/fail; exits 0 iff all pass.

set -eu

ROOT=$(cd "$(dirname "$0")" && pwd)
total_pass=0
total_fail=0

for vd in "$ROOT"/*/; do
    v=$(basename "$vd")
    case "$v" in
        lib) continue ;;
        [a-z]*) ;;
        *) continue ;;
    esac
    [ -f "$vd/run.sh" ] || continue
    if BIN="${BIN-}" TMP="${TMP-}" sh "$vd/run.sh"; then
        total_pass=$((total_pass + 1))
    else
        total_fail=$((total_fail + 1))
    fi
done

echo "suite: $total_pass verb(s) passed, $total_fail failed"
[ "$total_fail" -eq 0 ]
