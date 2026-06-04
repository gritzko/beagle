//  woof/fuzz/URI.c — open-once projection-Exec fuzzer (ASAN/stability).
//
//  Parses the fuzz bytes as an HTTP request-target the *same way woof
//  does* — WOOFutf8ExtractURI strips the leading '/' and percent-
//  decodes into a be-URI, then URIutf8Drain lexes it — then dispatches
//  the projection through woof's OWN in-process API path (WOOFApiOpen /
//  WOOFApiRun), routing by scheme to the owning dog (keeper/graf/spot/
//  sniff) exactly as `woof --api` does.  No fork.  Each dog is opened
//  once; every input re-runs <dog>Exec against the open store, its TLV
//  captured (fd → memfd) so the real write path executes under ASAN.
//
//  Single project: run from a worktree / .be root, e.g.
//      nice WOOFURIfuzz $HOME/Corpus/woof/URI -workers=8 -jobs=8
//
//  TODO(multi-project dispatch): woof's planned cross-project mode keeps
//  a per-project dog set open; this fuzzer serves the cwd's project.

#include "woof/WOOF.h"

#include "abc/BUF.h"
#include "abc/FILE.h"
#include "abc/PRO.h"
#include "abc/TEST.h"
#include "abc/URI.h"
#include "dog/DOG.h"
#include "dog/HOME.h"

#include <unistd.h>

//  wooflib's CONN.o (in-process dispatch) pulls in SNIFFExec, which
//  references the process argv global the MAIN macro normally defines.
//  FUZZ doesn't; provide an empty link-time stub (never read here).
u8cs *STD_ARGS[4] = {};

#define WOOF_FUZZ_OUT_CAP (1UL << 20)        // 1 MB projection-output sink

//  Reachable-from-globals across inputs → the leak detector never flags
//  the one-time keeper/graf/spot/sniff/home opens (LSan reports only
//  unreachable allocations).
static home WOOF_FUZZ_HOME = {};
static u8   WOOF_FUZZ_OUT[WOOF_FUZZ_OUT_CAP];
static u8   WOOF_FUZZ_DECODE[WOOF_REQBUF_BYTES];   // decoded be-URI bytes

//  Open every projector dog ONCE via woof's own --api entry point.
static b8 woof_fuzz_init(void) {
    static int state = 0;
    if (state) return state == 1;
    state = -1;
    if (FILEInit() != OK) return NO;
    //  One project: walk up from cwd to the `.be` anchor.
    if (HOMEOpenAt(&WOOF_FUZZ_HOME, (u8cs){}, NO) != OK) return NO;
    //  Point the woof singleton's home at ours and open the dogs through
    //  the real server entry — the fuzzer drives the identical code path
    //  (keeper/graf/spot/sniff + wtlog-tip forwarding).
    WOOF.h = &WOOF_FUZZ_HOME;
    if (WOOFApiOpen() != OK) return NO;
    state = 1;
    return YES;
}

//  One request inside a call/try frame so its BASS carves (and the
//  dogs', downstream) rewind on return — without the frame BASS would
//  accumulate across inputs until a_carve hits NOROOM.
static ok64 woof_fuzz_one(u8sc input) {
    sane(1);

    //  Parse the URI exactly as woof does: target = "/" + input, strip
    //  the slash, percent-decode into the be-URI buffer.
    a_carve(u8, tgt, WOOF_REQBUF_BYTES);
    call(u8bFeed1, tgt, '/');
    call(u8bFeed,  tgt, input);
    a_dup(u8c, target, u8bData(tgt));

    Bu8 dec = { WOOF_FUZZ_DECODE, WOOF_FUZZ_DECODE, WOOF_FUZZ_DECODE,
                WOOF_FUZZ_DECODE + sizeof(WOOF_FUZZ_DECODE) };
    call(WOOFutf8ExtractURI, dec, target);

    //  Lex; restore u.data to the full decoded slice (URIutf8Drain
    //  consumes it) so dispatch sees the canonical uri CLIParse provides.
    uri u = {};
    a_dup(u8c, beuri, u8bData(dec));
    call(URIutf8Drain, beuri, &u);
    a_dup(u8c, full, u8bData(dec));
    u.data[0] = full[0];
    u.data[1] = full[1];

    //  Route by scheme to the owning dog and run its Exec in-process —
    //  the same dispatch `woof --api` performs.
    char const *dog = DOGProjectorDog(u.scheme);
    if (dog == NULL || !WOOFApiDogOpen(dog)) done;   // not a routable projector
    call(WOOFApiRun, &u, dog);

    //  Drain the captured TLV into the 1 MB buffer (real write path; ASAN
    //  watches the copy).
    {
        int fd = WOOFApiMemfd();
        (void)lseek(fd, 0, SEEK_SET);
        (void)read(fd, WOOF_FUZZ_OUT, sizeof(WOOF_FUZZ_OUT));
    }
    done;
}

FUZZ(u8, WOOFURIfuzz) {
    sane(1);
    //  Keep "/" + input and the decode output (≤ input) within the 16 KB
    //  request region.
    if ($len(input) > (WOOF_REQBUF_BYTES / 2) - 2) done;
    if (!woof_fuzz_init()) done;

    //  Run in a try frame: BASS rewound on return; a parse / projection
    //  failure is an uninteresting (non-crash) input, swallowed so the
    //  FUZZ wrapper doesn't trap on a non-OK return.
    try(woof_fuzz_one, input);
    __ = OK;
    done;
}
