#   Coding guidelines

0.  Dependencies: libsodium, gtest, google-benchmark, libcurl.
      * Dirs for builds: build build-debug build-fuzz build-release build-asan.
        Recommended cmake conf:
         1. build-fuzz `CC=clang CXX=clang++ cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DWITH_ASAN=ON -DWITH_FUZZ=ON -DWITH_INET=OFF -GNinja -B. -S..`
         2. build-debug `CC=clang CXX=clang++ cmake -DCMAKE_BUILD_TYPE=Debug -DWITH_ASAN=ON -DWITH_INET=OFF -GNinja -B. -S..`
         3. build-release `CC=clang CXX=clang++ cmake -DCMAKE_BUILD_TYPE=Release -DWITH_INET=OFF -GNinja -B. -S..`
         2. build-trace `CFLAGS="-DABC_TRACE=ON" CC=clang CXX=clang++ cmake -DCMAKE_BUILD_TYPE=Debug -DWITH_ASAN=ON -DWITH_INET=ON -GNinja -B. -S..`
        When running with tracing on, always redirect to a file, then tail/head/grep it.
      * clang and llvm are preferred to gcc and gdb.
        ripgrep is preferred over grep.
      * ragel invocation pattern: `ragel -C I3.c.rl -o I3.rl.c -L`, avoid other modes.
      * running tests: ctest -j16, timeout 1 minute 
    There is a fast CI script at scripts/ci-fast.sh to check for regressions 
    after you edit things. When introducing new behavior, always add
    tests, preferably by adding cases to existing table driven tests.
1.  Please follow the ABC coding style:
      - no pointer arithmetics, use slices where possible,
        u8 pad[64] is not OK, a_pad(u8, pad, 64) is OK (has end pointer).
        see abc/S.md abc/B.md on slices and buffers.
        Note that slices/gauges/buffers track data consumption naturally, so
        ptr+len pairs are highly discouraged, as well as consumed-len pointers.
      - prefer typed slice functions over generic macros (e.g. `u8bReset()` over 
        `Breset()` or `u8sMv()` over `$mv()` over direct access s1[0] = s2[0]),
      - use slice iteration where possible, `$for`, `$rof`, `$eat`
      - use slice/buffer macros where applicable: a_head, a_tail, a_rest, a_pad
        Again, avoid any pointer arithmetics.
      - stdlib legacy use is discouraged
      - POSIX calls are normally wrapped (e.g. FILE.h, POL.h)
      - utf8s for text slices, u8s for binary
      - use abc/PATH over manual path construction, starting with a_pad()
      - PRO.h can not be imported by public headers as it pollutes the namespace.
      - note that ABC APIs like to *consume* slices; that is often signaled
        through type (u8cs consumed, u8csc is not); use that
        To use PRO.h macros (call, done, etc) a function must start with sane() check.
        Also, the main() has to be MAIN(), TEST() or FUZZ() to declare PRO.h globals.
2.  Function naming convention: `MOD typ8 VerbStuff ()`, e.g. `HEXu8sFeed()` 
      - MOD is the module (header)
      - ABC record types have fixed bit layout, their name most often ends
        with the bit width, e.g. `sha256`
      - Verb is the function name per se
      - Stuff may be combinatorial (the Go way), e.g. see `FeedXYZ()` functions
3.  For anything implemented we must ensure it actually works (test, bench, fuzz).
    Each project has `test/`, `fuzz/` and `bench/` subdirs, follow that pattern.
      - tests: preferably Go-style, table driven property tests.
        It helps to fill that table before you start implementing the feature itself.
      - fuzz tests: based on respective property tests. Fuzz fails, once minimized, 
        get fed back into property tests to prevent regressions. Use clang/libfuzzer.
        On long runs, use all cores, put the corpus into appropriate $HOME/Corpus/ or ./Corpus dir.
        Run fuzzer with `nice`. Use `must()`, not `assert()`. I repeat:
          * fuzz, find a crash
          * minimize
          * add a repro to the tests
          * fix
          * keep fuzzing
      - benchmarks: test important performance parameters, preferably in a way 
        that makes regressions brutally obvious.
4.  When summarizing texts and reporting the state of things, write 1 paragraph, no more.
5.  Resource allocation (alloc, map, file open) normally happens in the very top
    of the call chain. Anything downstream works with the provided resources. Still,
    any functions that take some resource should be examined for leaks. Pay attention
    to the call()/try()/then try() pattern. Separating resource allocation and use
    into different functions is recommended.
6.  Funny macros from PRO.h should not pollute namespaces, hence that header must not
    be included by any other headers, only .c files.
7.  Error codes are uppercase, follow predictable pattern, numeric values are
    ron60 coded, see abc/ok64 utility. 10 chars max (60 bits in RON base 64)
8.  Use different build dirs: build/ for debug, build-release/ for bench, build-fuzz
    for fuzzing and build-load (release build, heavy long running tests),
    all under the project root, e.g. librdx/build, also see WITH_ASAN, WITH_FUZZ,
    WITH_LOAD in the CMakeLists.txt.
9.  Read spot/README.md. Use spot for search and mass-refactorings.
10. No pointer arithmetics. None. Just don't. There are all sorts of typed slice
    handling functions and macros, don't calculate any pointers manually.
11. Avoid cat-ting large files and *never* send traces to stdout. Redirect to a file,
    then grep/tail the file.
12. Read ABC.md and \*/INDEX.md. Reread them after every compaction.
13. Dont reimplement things repeatedly. When planninng, check `*/INDEX.md` for 
    a relevant header, the thing may already exist. Always ask if unsure.
14. After large code changes (esp. new files added) update respective INDEX.md.
15. Read abc/S.md abc/B.md for the idiomatic slice manipulation functions
16. Propagate errors. Check error codes where correcting action is possible.
    Use sane()/call()/try()/done cycle from PRO.h in .c files.
17. When fixing a bug, first make a repro test, then fix a bug.
    Check your fix actually works.
