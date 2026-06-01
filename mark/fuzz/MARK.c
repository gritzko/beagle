//  mark stability fuzzer.
//
//  Treats arbitrary input bytes as StrictMark and renders them to HTML.
//  The only contract is "must not crash" — render errors (malformed
//  markup, budget breaches) are fine and are swallowed; ASAN/UBSAN or a
//  segfault is the failure signal.
//
//  Build: configure with -DWITH_FUZZ=ON -DWITH_ASAN=ON (build-fuzz).
//  Run:   nice ./bin-or-path/MARKfuzz -jobs=16 -workers=16 \
//             -max_total_time=600 Corpus/mark

#include "mark/MARK.h"

#include "abc/PRO.h"
#include "abc/TEST.h"

FUZZ(u8, MARKfuzz) {
    sane(1);
    if ($len(input) > (1u << 16)) done;  // bound size (and inline recursion)

    u8b out = {};
    size_t cap = (size_t)$len(input) * 8 + 8192;
    try(u8bAllocate, out, cap);
    nedo return OK;  // allocation failure is not a crash

    markopts opts = {};  // non-strict: budget breaches must not fail the fuzzer
    u8cs title = {};
    try(MARKRenderDoc, out, input, title, opts);

    u8bFree(out);
    return OK;  // reached iff no crash; render status is intentionally ignored
}
