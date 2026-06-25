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

enable_testing()
add_subdirectory(test)
