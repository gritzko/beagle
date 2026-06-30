# beagle-ext (`be/`) — the JavaScript beagle extension: the resident dispatch
# loop (main.js + core/ + verbs/ + views/) run by `jab`, plus its parity test
# suite (test/, the beagle-ext-tests submodule).
#
# This project builds NOTHING: the toolchain it tests against — `be`, `jab`,
# `keeper` — is EXTERNAL, produced by the beagle submodule.  So `be/` is pure
# test orchestration over a prebuilt toolchain, and stays independently runnable:
#   cmake -S . -B build [-DBEAGLE_BIN_DIR=/path/to/beagle/build/bin] && ctest --test-dir build
cmake_minimum_required(VERSION 3.20)
project(beagle_ext NONE)

# The beagle binaries live in their OWN build tree (never rebuilt here).  Default
# to the in-tree debug build at ../beagle/build/bin (the journal superlayout);
# override for a release/clone build dir, or let the journal root pass it through.
get_filename_component(_be_default_bin
    "${CMAKE_CURRENT_SOURCE_DIR}/../beagle/build/bin" ABSOLUTE)
set(BEAGLE_BIN_DIR "${_be_default_bin}" CACHE PATH
    "Directory holding the prebuilt beagle binaries (be, jab, keeper)")

foreach(_tool be jab keeper)
    if (NOT EXISTS "${BEAGLE_BIN_DIR}/${_tool}")
        message(WARNING
            "be/: '${_tool}' not found in ${BEAGLE_BIN_DIR}.\n"
            "  Build the beagle toolchain first (e.g. cmake --build ../beagle/build),\n"
            "  or pass -DBEAGLE_BIN_DIR=<dir with be/jab/keeper>.  Tests error until then.")
    endif()
endforeach()

# Each case is PID-isolated ($TMP/$$/…) and spends its wall-clock spawning many
# short-lived processes (jab pays ~65ms engine init per one-shot store read/op;
# be/sniff/keeper shell out to each other), so the suite is process-spawn-bound,
# NOT CPU-bound.  Run cases in PARALLEL by default — `make test` / `ctest` then
# use the box (≈8× here: 83s→10s).  Tune with -DBE_TEST_JOBS=N (1 = serial).
cmake_host_system_information(RESULT _be_ncpu QUERY NUMBER_OF_LOGICAL_CORES)
if (NOT _be_ncpu OR _be_ncpu LESS 1)
    set(_be_ncpu 4)
endif()
set(BE_TEST_JOBS "${_be_ncpu}" CACHE STRING
    "Parallel test jobs for ctest / make test (1 = serial)")
# Default `make test` / `ctest` EXCLUDES the `big`-labelled treadmill cases (the
# two heavy wire clones of the real git tree) — they run on demand via `mill`
# below (or `ctest -L big`).  Everything else stays in the default green set.
set(CMAKE_CTEST_ARGUMENTS
    "--parallel;${BE_TEST_JOBS};--output-on-failure;--label-exclude;big")

enable_testing()
add_subdirectory(test)

# `mill` — run ONLY the `big` treadmill (kept out of `make test`).  `-L big`
# selects the two labelled wire cases; `ninja mill` / `make mill` runs them.
add_custom_target(mill
    COMMAND ${CMAKE_CTEST_COMMAND} -L big --output-on-failure
            --parallel ${BE_TEST_JOBS}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    USES_TERMINAL
    COMMENT "Running the big treadmill wire cases (ctest -L big)")
