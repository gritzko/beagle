add_executable(DAG01test DAG01.c)
target_link_options(DAG01test PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(DAG01test PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(DAG01test graflib dog abc-core)
add_test(NAME DAG01test COMMAND DAG01test)

add_executable(JOIN01test JOIN01.c)
target_link_options(JOIN01test PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(JOIN01test PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(JOIN01test graflib dog abc-core)
add_test(NAME JOIN01test COMMAND JOIN01test)

add_executable(WEAVE01test WEAVE01.c)
target_link_options(WEAVE01test PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(WEAVE01test PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(WEAVE01test graflib dog abc-core)
add_test(NAME WEAVE01test COMMAND WEAVE01test)

add_executable(WEAVE2test WEAVE2.c)
target_link_options(WEAVE2test PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(WEAVE2test PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(WEAVE2test graflib dog abc-core)
add_test(NAME WEAVE2test COMMAND WEAVE2test)

add_executable(NEIL01test NEIL01.c)
target_link_options(NEIL01test PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(NEIL01test PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(NEIL01test graflib dog abc-core)
add_test(NAME NEIL01test COMMAND NEIL01test)

add_executable(BRAM02test BRAM02.c)
target_link_options(BRAM02test PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(BRAM02test PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(BRAM02test graflib dog abc-core)
add_test(NAME BRAM02test COMMAND BRAM02test)

add_executable(REBASE01test REBASE01.c)
target_link_options(REBASE01test PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(REBASE01test PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(REBASE01test graflib dog keeplib abc-core)
add_test(NAME REBASE01test COMMAND REBASE01test)
set_tests_properties(REBASE01test PROPERTIES
    ENVIRONMENT "TMP=${DOG_TEST_TMP_ROOT};TEST_ID=REBASE01test")

add_executable(REBASE02test REBASE02.c)
target_link_options(REBASE02test PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(REBASE02test PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(REBASE02test graflib dog keeplib abc-core)
add_test(NAME REBASE02test COMMAND REBASE02test)
set_tests_properties(REBASE02test PROPERTIES
    ENVIRONMENT "TMP=${DOG_TEST_TMP_ROOT};TEST_ID=REBASE02test")

add_executable(MERGEWT01test MERGEWT01.c)
target_link_options(MERGEWT01test PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(MERGEWT01test PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(MERGEWT01test graflib dog keeplib abc-core)
add_test(NAME MERGEWT01test COMMAND MERGEWT01test)
set_tests_properties(MERGEWT01test PROPERTIES
    ENVIRONMENT "TMP=${DOG_TEST_TMP_ROOT};TEST_ID=MERGEWT01test")

#  MMAP01 — MEM-040 repro: own-open verbs must GRAFClose on error.
#  Counts /proc/self/maps VMAs across N starved error-returns.
add_executable(MMAP01test MMAP01.c)
target_link_options(MMAP01test PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(MMAP01test PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(MMAP01test graflib dog keeplib abc-core)
add_test(NAME MMAP01test COMMAND MMAP01test)
set_tests_properties(MMAP01test PROPERTIES
    ENVIRONMENT "TMP=${DOG_TEST_TMP_ROOT};TEST_ID=MMAP01test")

add_executable(MERGE3B01test MERGE3B01.c)
target_link_options(MERGE3B01test PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(MERGE3B01test PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(MERGE3B01test graflib dog abc-core)
add_test(NAME MERGE3B01test COMMAND MERGE3B01test)

#  toy.sh — full-pipeline smoke test: be post x3, merge, blame,
#  weave, index/status.  Exercises dog/DOG.md §8 entry points.
add_test(NAME GRAFtoy
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/toy.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(GRAFtoy PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=GRAFtoy")

#  get.sh — URI-driven deterministic blob merge (`graf get <uri>`).
add_test(NAME GRAFget
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/get.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(GRAFget PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=GRAFget")

#  blame-perf.sh — BLAME-001 guard: blob inflated ~per-change, not
#  ~per-ancestor (asserts GRAF_BLAME_STATS blobfetch is bounded).
add_test(NAME GRAFblameperf
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/blame-perf.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(GRAFblameperf PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=GRAFblameperf")

#  blame-identical.sh — BLAME-002 correctness: index-served path descent
#  (DAGChildStep over materialised (tree,name)→child edges) is
#  byte-identical to the keeper-inflate descent (GRAF_BLAME_NOINDEX), and
#  index-served treeinfl is 0.
add_test(NAME GRAFblameident
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/blame-identical.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(GRAFblameident PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=GRAFblameident")

#  blame-bignoroom.sh — BLAME-004 guard: a large, long-history file must
#  blame OK, not BNOROOM.  Pre-fix the per-fold WEAVE BASS scratch was never
#  rewound (bare calls in GRAFFileWeave) and piled up until ABC_BASS hit its
#  1 GB cap mid-walk; the fix folds via try() so BASS rewinds per version.
add_test(NAME GRAFblamebignoroom
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/blame-bignoroom.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(GRAFblamebignoroom PROPERTIES
    TIMEOUT 150
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=GRAFblamebignoroom")

#  blame-runs.sh — BLAME-005: `blame:` emits one content hunk per maximal
#  commit-run (uri = commit:?<sha40>, ts = commit time, body = the run's
#  verbatim source + syntax toks), replacing the pre-fix per-line gutter.
add_test(NAME GRAFblameruns
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/blame-runs.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(GRAFblameruns PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=GRAFblameruns")

#  blame-readd.sh — BLAME-006a: a file DELETED then RE-ADDED with
#  structurally-similar content (title + blank lines) must blame to the
#  re-add commit, never leak the title/blanks back to the ROOT commit.
#  Pre-fix GRAFFileWeave skipped the delete (BLAME.c:480/492 continue)
#  instead of folding an empty all-delete layer, so old tokens stayed
#  alive and the readd inherited their root seq via the WEAVE EQ branch.
add_test(NAME GRAFblamereadd
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/blame-readd.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(GRAFblamereadd PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=GRAFblamereadd")

if (WITH_SSH)
    #  mill-merges.sh — 3-way merge treadmill against curated merges in
    #  ~/src/git.  Each case fetches the two parents via keeper (ssh
    #  localhost, same transport as mill-tags.sh) and compares
    #  `graf get <path>?<P1>&<P2>` against `git show <merge>:<path>`.
    #  Heavy (pulls real history) — ctest runs it only when explicitly
    #  selected.  Toggle via `-DGRAF_MILL_MERGES=ON` or set it in the
    #  environment.
    option(GRAF_MILL_MERGES "Register the graf merge-treadmill test" OFF)
    if (GRAF_MILL_MERGES)
        add_test(NAME GRAFmillMerges
            COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/mill-merges.sh
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        set_tests_properties(GRAFmillMerges PROPERTIES
            ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=GRAFmillMerges")
    endif()
endif()

add_executable(BRAM01test BRAM01.c)
target_link_options(BRAM01test PRIVATE ${TEST_LDD_FLAGS})
target_compile_options(BRAM01test PRIVATE ${TEST_CXX_FLAGS})
target_link_libraries(BRAM01test graflib dog abc-core)
add_test(NAME BRAM01test COMMAND BRAM01test)
