# test/lib/verb.sh — sourced by every <verb>/run.sh.
#
# Provides run_verb VERB which discovers <verb>/<case>/run.sh files
# under the current verb dir, runs each in a child shell, and prints
# OK/FAIL per case.  Exits 0 iff every case passes.
#
# Parallelism: $JOBS controls case-level concurrency (default = nproc).
# Set JOBS=1 for the old serial behaviour; useful when debugging a
# specific test or when output ordering matters.  Per-case stdout/
# stderr is buffered to a file and dumped only on FAIL so parallel
# logs stay readable.

set -eu

run_verb() {
    _verb=$1
    _verbdir=$(cd "$(dirname "$0")" && pwd)
    _jobs=${JOBS:-$(nproc 2>/dev/null || echo 4)}
    _runs=$(ls "$_verbdir"/*/run.sh 2>/dev/null | sort)
    [ -z "$_runs" ] && { echo "verb $_verb: no cases"; return 0; }

    _logdir=$(mktemp -d)
    trap 'rm -rf "$_logdir"' EXIT

    #  Worker: run one case, write its log to <logdir>/<case>.{log,rc}.
    #  Stdout is just the status line ("OK …" / "FAIL …") — short
    #  enough (≤ PIPE_BUF) that concurrent workers' lines don't
    #  interleave.  Full failure logs go to disk and are dumped after
    #  the parallel sweep.
    _worker='
        case=$(basename "$(dirname "$1")")
        log="$2/$case.log"
        rcf="$2/$case.rc"
        if BIN="${BIN-}" TMP="${TMP-}" sh "$1" >"$log" 2>&1; then
            printf "OK   %s/%s\n" "$3" "$case"
            echo 0 > "$rcf"
            exit 0
        else
            printf "FAIL %s/%s\n" "$3" "$case"
            echo 1 > "$rcf"
            exit 1
        fi
    '

    #  `set -e` would abort here when xargs returns 123 (any worker
    #  failed) — we want to continue and aggregate logs.  `|| true`
    #  swallows the exit code; the per-case .rc files carry the truth.
    printf '%s\n' $_runs | \
        xargs -P "$_jobs" -I{} sh -c "$_worker" _ {} "$_logdir" "$_verb" \
        || true

    _pass=0; _fail=0
    for _rcf in "$_logdir"/*.rc; do
        [ -f "$_rcf" ] || continue
        _rc=$(cat "$_rcf")
        if [ "$_rc" = 0 ]; then
            _pass=$((_pass + 1))
        else
            _fail=$((_fail + 1))
            #  Dump the failing case's full log under a header so
            #  each failure is a self-contained block in the output.
            _case=$(basename "$_rcf" .rc)
            printf "\n--- FAIL %s/%s ---\n" "$_verb" "$_case"
            sed "s|^|    |" "$_logdir/$_case.log"
        fi
    done

    echo "verb $_verb: $_pass passed, $_fail failed (jobs=$_jobs)"
    [ "$_fail" -eq 0 ]
}
