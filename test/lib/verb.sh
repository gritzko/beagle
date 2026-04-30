# test/lib/verb.sh — sourced by every <verb>/run.sh.
#
# Provides run_verb VERB which discovers <verb>/<case>/run.sh files
# under the current verb dir, runs each in a child shell, and prints
# OK/FAIL per case.  Exits 0 iff every case passes.

set -eu

run_verb() {
    _verb=$1
    _verbdir=$(cd "$(dirname "$0")" && pwd)
    _pass=0
    _fail=0
    # Sorted by case dir name so output is stable.
    for _case_run in $(ls "$_verbdir"/*/run.sh 2>/dev/null | sort); do
        _case=$(basename "$(dirname "$_case_run")")
        if BIN="${BIN-}" TMP="${TMP-}" sh "$_case_run"; then
            echo "OK   $_verb/$_case"
            _pass=$((_pass + 1))
        else
            echo "FAIL $_verb/$_case"
            _fail=$((_fail + 1))
        fi
    done
    echo "verb $_verb: $_pass passed, $_fail failed"
    [ "$_fail" -eq 0 ]
}
