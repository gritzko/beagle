enable_language(CXX)

if(APPLE)
    find_library(JSC_LIB JavaScriptCore)
    set(JSC_INCLUDE_DIR "")
else()
    find_library(JSC_LIB
        NAMES javascriptcoregtk-4.1 javascriptcoregtk-4.0
        PATHS /usr/lib/x86_64-linux-gnu/ /usr/lib
    )
    find_path(JSC_INCLUDE_DIR
        NAMES JavaScriptCore/JSBase.h
        PATH_SUFFIXES webkitgtk-4.1/ webkitgtk-4.0/
    )
endif()

if(NOT JSC_LIB)
    message(WARNING "js: JavaScriptCore not found, skipping")
    return()
endif()

include_directories(
    ${JSC_INCLUDE_DIR}
    ..
    .
)

# The JS Buf class is embedded in buf.cpp as a raw string literal; no js2c /
# node codegen step (node is not a build dependency).
set(JABC_SRC main.cpp io.cpp utf8.cpp buf.cpp cont.cpp uri.cpp codec.cpp require.cpp)

add_executable(jabc ${JABC_SRC})
target_link_libraries(jabc ${JSC_LIB} dog abc-core ${sodium})

# Table-driven C++ harness over the buf/utf8/io boundary; returns non-zero on
# any failed row, so CTest fails on a real bug (CLAUDE.md §3).
add_executable(jabc_test test/jabc_test.cpp io.cpp utf8.cpp buf.cpp)
target_link_libraries(jabc_test ${JSC_LIB} abc)

add_test(NAME JABCtestCpp COMMAND jabc_test)

# End-to-end: the jabc binary runs an inline Buf+utf8 round-trip; any failed
# assertion throws, which JABCRun turns into a non-zero exit (CTest fails).
add_test(NAME JABCe2e COMMAND jabc --eval
    "let b=io.buf(16);b.feedStr('hié');if(b.size!==4)throw 'size';if(utf8.Decode(b.data())!=='hié')throw 'roundtrip';if(b.take(1)[0]!==104)throw 'take';if(b.size!==3)throw 'consume'")

# Container bindings (abc.* HEAP/HASH over all lanes) — each .js throws on a
# failed assertion, which JABCRun turns into a non-zero exit (CTest fails).
add_test(NAME JABCheap COMMAND jabc ${CMAKE_CURRENT_SOURCE_DIR}/test/heap.js)
add_test(NAME JABChash COMMAND jabc ${CMAKE_CURRENT_SOURCE_DIR}/test/hash.js)
add_test(NAME JABCmmap COMMAND jabc ${CMAKE_CURRENT_SOURCE_DIR}/test/mmap.js)
add_test(NAME JABCsha COMMAND jabc ${CMAKE_CURRENT_SOURCE_DIR}/test/sha.js)
add_test(NAME JABChit COMMAND jabc ${CMAKE_CURRENT_SOURCE_DIR}/test/hit.js)
add_test(NAME JABChunk COMMAND jabc ${CMAKE_CURRENT_SOURCE_DIR}/test/hunk.js)
add_test(NAME JABCuri COMMAND jabc ${CMAKE_CURRENT_SOURCE_DIR}/test/uri.js)
add_test(NAME JABCcodec COMMAND jabc ${CMAKE_CURRENT_SOURCE_DIR}/test/codec.js)
add_test(NAME JABCrequire COMMAND jabc ${CMAKE_CURRENT_SOURCE_DIR}/test/require.js)
add_test(NAME JABCpack COMMAND jabc ${CMAKE_CURRENT_SOURCE_DIR}/test/pack.js)

# JavaScriptCore leaks lazily-initialised global singletons that LSan flags
# but are not our leaks; suppress JSC-internal allocations by library name.
set_tests_properties(JABCtestCpp JABCe2e JABCheap JABChash JABCmmap JABCsha JABChit JABChunk JABCuri JABCcodec JABCrequire JABCpack PROPERTIES
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/lsan.supp")
