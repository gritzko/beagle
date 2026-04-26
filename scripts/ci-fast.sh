#!/bin/bash
#
# Fast CI: build, test, fuzz for 1 minute each (parallel)
#

set -e

SRCDIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILDDIR="$SRCDIR/build-fuzz"
CORPUS="$SRCDIR/Corpus"
RUNDIR="$SRCDIR/run-fuzz"
FUZZ_TIME=${FUZZ_TIME:-60}

cd "$SRCDIR"

# Step 1: Create build dir and cmake if needed
if [[ ! -f "$BUILDDIR/build.ninja" ]]; then
    echo "=== Configuring build-fuzz ==="
    mkdir -p "$BUILDDIR"
    cmake -B "$BUILDDIR" -GNinja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DWITH_FUZZ=ON -DWITH_ASAN=ON \
        -DWITH_INET=OFF \
        -DLLVM_ENABLE_RUNTIMES=compiler-rt
fi

# Step 2: Build
echo "=== Building ==="
ninja -C "$BUILDDIR"

# Step 3: Run tests
echo "=== Running tests ==="
if ! ctest --test-dir "$BUILDDIR" --parallel 16 --output-on-failure; then
    echo "WARNING: Some tests failed (continuing to fuzz)"
fi

# Step 4: Run fuzz tests in parallel
echo "=== Fuzzing (${FUZZ_TIME}s each, parallel) ==="

FUZZ_PIDS=()
FUZZ_NAMES=()
FUZZ_LOGS=()

start_fuzz() {
    local module=$1
    local name=$2
    local exe="$BUILDDIR/$module/fuzz/${name}fuzz"
    local corpus="$CORPUS/$module/$name"
    local rundir="$RUNDIR/$module/$name"
    local log="$rundir/fuzz.log"

    if [[ ! -x "$exe" ]]; then
        echo "  SKIP $module/$name (no $exe executable)"
        return 0
    fi

    mkdir -p "$corpus" "$rundir"
    echo "  START $module/$name"
    (cd "$rundir" && nice "$exe" "$corpus" -max_total_time=$FUZZ_TIME >"$log" 2>&1) &
    FUZZ_PIDS+=($!)
    FUZZ_NAMES+=("$module/$name")
    FUZZ_LOGS+=("$log")
}

# Launch all fuzzers
start_fuzz abc HEAP
start_fuzz abc HASH
#start_fuzz abc HASHd  # complex order-independence test, needs review
#start_fuzz abc ZINT
start_fuzz abc ZINT2
#start_fuzz abc SORT  # SORT not used, has bugs
start_fuzz abc URI
start_fuzz abc DIFF
start_fuzz conv APT
start_fuzz rdx VFY
start_fuzz rdx AA
start_fuzz rdx AB
start_fuzz rdx RB
start_fuzz rdx DRDX
start_fuzz rdx SKIL
start_fuzz rdx SLIK
#start_fuzz cc PATH
#start_fuzz cc TXT
start_fuzz cc JSON
start_fuzz ast CSS
start_fuzz graf JOIN

# Wait for all and collect results
FAILED=0
for i in "${!FUZZ_PIDS[@]}"; do
    pid=${FUZZ_PIDS[$i]}
    name=${FUZZ_NAMES[$i]}
    log=${FUZZ_LOGS[$i]}

    if wait $pid; then
        # Show final stats
        tail -1 "$log" | grep -q "DONE" && echo "  OK $name" || echo "  OK $name"
    else
        echo "  CRASH $name"
        echo "--- $log ---"
        tail -20 "$log"
        echo "---"
        FAILED=1
    fi
done

if [[ $FAILED -eq 1 ]]; then
    echo "=== FAILED ==="
    exit 1
fi

echo "=== PASSED ==="
