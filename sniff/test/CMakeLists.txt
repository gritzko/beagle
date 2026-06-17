add_executable(SNIFFtest SNIFF.c)
target_link_libraries(SNIFFtest snifflib)
add_test(NAME SNIFFtest COMMAND SNIFFtest)

add_test(NAME SNIFFworkflow
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/workflow.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(SNIFFworkflow PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=SNIFFworkflow")

#  squash.sh — concurrent trunk + sub-branch edits folded back via
#  `sniff patch` + `sniff post` (one linear commit on the parent).
add_test(NAME SNIFFsquash
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/squash.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(SNIFFsquash PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=SNIFFsquash")

#  ls.sh — `sniff ls` / `be get ls:` smoke test (three plain-text
#  modes + TLV framing sanity + non-destructive be routing).
add_test(NAME SNIFFls
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/ls.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(SNIFFls PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=SNIFFls")

#  dirty.sh — GET pre-flight: cross-branch dirty refuse + same-branch
#  overlap refuse + untracked-only carry-over + rollback semantics.
add_test(NAME SNIFFdirty
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/dirty.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(SNIFFdirty PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=SNIFFdirty")

#  idempotent.sh — `be`/`sniff status` is idempotent: a genuinely
#  modified file stays `mod` across re-runs (no mtime-only flip to
#  `ok`), content-confirmed against the baseline blob (DIS-023).
add_test(NAME SNIFFidempotent
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/idempotent.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(SNIFFidempotent PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=SNIFFidempotent")

#  post-ff.sh — POST pre-flight: empty-commit refuse + non-ff refuse
#  (REFS tip vs .be/wtlog base divergence).
add_test(NAME SNIFFpostff
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/post-ff.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(SNIFFpostff PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=SNIFFpostff")

#  relref.sh — `?./X`, `?../X`, `?..` resolve via .be/wtlog baseline.
add_test(NAME SNIFFrelref
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/relref.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(SNIFFrelref PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=SNIFFrelref")

#  shareset.sh — GET-027: `put '?#<sha>'` records a single-sigil
#  `?#<sha>` row, never the corrupt double-sigil `?#?<sha>`.
add_test(NAME SNIFFshareset
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/shareset.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(SNIFFshareset PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=SNIFFshareset")

#  fragref.sh — GET-023: `be get '?#<sha>'` honours the fragment and a
#  malformed fragment (`?#<sha>...` trailing dots, non-hex word) is
#  REFUSED instead of silently landing on the wrong commit / trunk.
add_test(NAME SNIFFfragref
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/fragref.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(SNIFFfragref PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=SNIFFfragref")

#  branchdel.sh — `sniff delete ?branch` writes a tombstone +
#  safety checks (current-wt, descendants, resurrection).
add_test(NAME SNIFFbranchdel
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/branchdel.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(SNIFFbranchdel PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=SNIFFbranchdel")

#  cross-post.sh — `sniff post -m "msg" ?<target>` lands the new
#  commit on target only (other branches in REFS unchanged) +
#  non-ff cross-branch refusal.
add_test(NAME SNIFFcrosspost
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/cross-post.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(SNIFFcrosspost PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=SNIFFcrosspost")

if (WITH_SSH)
    #  patch.sh — 3-way merge toys (sniff patch <ref>).  Needs ssh
    #  localhost (same transport as beagle/test/blobs-from-git.sh).
    add_test(NAME SNIFFpatch
        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/patch.sh
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(SNIFFpatch PROPERTIES
        ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=SNIFFpatch")

    #  post-roundtrip.sh — be get / be post / be put //origin end-to-end:
    #  clone, edit, commit, push back, verify the bare remote's git
    #  log + tree + parent linkage + pack-size budget.
    #  Coverage moved to beagle/test/roundtrip.sh (BEroundtrip);
    #  the standalone script never landed.
endif()

#  refuse.sh — coverage for the new refusal codes (PUTNONE on
#  baseline-clean / empty bare-put; DELDIRTY on user-edited).
add_test(NAME SNIFFrefuse
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/refuse.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(SNIFFrefuse PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=SNIFFrefuse")

#  restore.sh — single-file `sniff get <path>` baseline restore for
#  subdir paths (regression: PATHu8bPush rejected embedded '/' → PATHBAD).
add_test(NAME SNIFFrestore
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/restore.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(SNIFFrestore PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=SNIFFrestore")

#  norepo.sh — bare `sniff` outside any repo refuses cleanly and never
#  bootstraps a stray `.be/` (regression: $HOME-tree scan / hang).
add_test(NAME SNIFFnorepo
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/norepo.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(SNIFFnorepo PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=SNIFFnorepo")

#  contam.sh — POST-016 regression: a selective `sniff post` commits
#  EXACTLY the staged set, never a file whose mtime resolves to an
#  out-of-scope (leftover/aborted-patch) stamp, and never an unrelated
#  dirty tracked file.
add_test(NAME SNIFFcontam
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/contam.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(SNIFFcontam PROPERTIES
    ENVIRONMENT "BIN=${DOG_BIN_DIR};TMP=${DOG_TEST_TMP_ROOT};TEST_ID=SNIFFcontam")
