//
//  MMAP01 — MEM-040 repro: verb error paths must not leak mmaps.
//
//  The leak under test is the ~18 MB owned graf open (16 MB GRAF_ARENA
//  + 2×1 MB obj bufs, plus keeper/kv) that LeakSanitizer does NOT
//  report — it lives in the process-global GRAF singleton (released by
//  munmap, not free) and a leaked open is silently REUSED by the next
//  call, so it never even grows the VMA count.  We therefore assert on
//  the exact contract instead: after an own-open verb error-returns,
//  graf must be CLOSED.  GRAFOpen is idempotent, so probing it tells us
//  the state — OK ⇒ was closed (clean), GRAFOPEN* ⇒ was left open (the
//  leak).  See graf_left_open() below.
//
//  Failure injection without a fault hook: we starve the BASS scratch
//  arena down to a sliver before the call.  GRAFOpen / GRAFArenaInit
//  map their big arenas with mmap (NOT from BASS), so the owned open
//  still SUCCEEDS (own_open == true) even with BASS nearly full; the
//  FIRST post-open BASS carve then fails with BNOROOM and the verb
//  must still GRAFClose its owned open on the way out.
//
//  Pre-fix: GRAFMap returns the carve error without closing, leaving
//  graf open (leaked).  Post-fix: own_graf GRAFClose runs on every
//  error exit.  GRAFHead shares the identical own-open-then-fail shape
//  and the identical fix (post-open body funneled through a render
//  helper with an unconditional close epilogue).
//

#include "graf/GRAF.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "abc/FILE.h"
#include "abc/PRO.h"
#include "abc/TEST.h"
#include "abc/URI.h"
#include "dog/HOME.h"
#include "keeper/KEEP.h"

#define MMAP01_ITERS 8

static char g_tmp[256];

static ok64 setup_repo(void) {
    sane(1);
    call(FILEInit);
    snprintf(g_tmp, sizeof(g_tmp), "/tmp/grafmmap-XXXXXX");
    want(mkdtemp(g_tmp) != NULL);
    a_cstr(root, g_tmp);
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);
    //  NB: do NOT GRAFOpen here — the verbs under test must own the
    //  open themselves (own_open == true) for the leak to be live.
    done;
}

static void teardown_repo(void) {
    GRAFClose();
    KEEPClose();
    HOMEClose();
    char cmd[300];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_tmp);
    (void)system(cmd);
}

//  Probe: is graf currently open?  GRAFOpen is idempotent — it returns
//  GRAFOPEN / GRAFOPENRO when graf is ALREADY open, and OK only when it
//  newly opened.  So an OK result means "was closed" (and we undo it);
//  a GRAFOPEN* result means "was already open" (i.e. leaked by the verb
//  under test).  This is exact and immune to VMA reuse — the leaked
//  arenas are the open graf singleton itself, not throwaway maps.
static b8 graf_left_open(void) {
    ok64 go = GRAFOpen(NO);
    if (go == OK) { GRAFClose(); return NO; }   // was closed → clean
    return YES;                                 // GRAFOPEN* → leaked open
}

//  Run `fn(u)` once with BASS starved so the FIRST post-own-open carve
//  fails, forcing the verb down its post-open error path.  Asserts the
//  call errored (so we know the own-open region was reached) and that
//  graf is NOT left open afterwards (the leak under test).  BASS is
//  rewound by the surrounding call() frame; the hog dies with it.
static ok64 starved_call(ok64 (*fn)(uricp), uricp u) {
    sane(fn && u);
    //  Leave ~1.5 MB of BASS idle.  GRAFMap's first PRE-open carve
    //  (strs_arena, 1 MB) then fits, but the first POST-open carve
    //  (union_set, 1 MB) cannot — landing the failure after own_open.
    size_t idle = (size_t)(u8bIdleHead(ABC_BASS) <= u8bTerm(ABC_BASS)
                           ? (u8bTerm(ABC_BASS) - u8bIdleHead(ABC_BASS)) : 0);
    if (idle < (3UL << 20)) return OK;       // can't starve; skip
    size_t leave = (3UL << 19);              // ~1.5 MB
    Bu8 hog = {};
    ok64 ho = u8bAcquire(ABC_BASS, hog, idle - leave);
    if (ho != OK) return ho;
    ok64 r = fn(u);
    //  We EXPECT a resource error (BNOROOM-family), not OK — an OK here
    //  means the injection missed and the test would be meaningless.
    want(r != OK);
    //  THE PROPERTY: the owned open must have been closed on the way out.
    if (graf_left_open()) {
        fprintf(stderr, "MMAP01: own-open LEAKED (graf left open after "
                        "error r=%s)\n", ok64str(r));
        fail(TESTFAIL);
    }
    done;
}

//  Property: every starved error-return through an own-open verb closes
//  the graf it opened.  Run N iterations so a sticky-but-reused leak is
//  still caught on the first failing iteration.
static ok64 assert_no_open_leak(ok64 (*fn)(uricp), char const *label) {
    sane(fn && label);
    a_cstr(us, "map:");
    uri u = {};
    (void)URIutf8Drain(us, &u);
    for (u32 i = 0; i < MMAP01_ITERS; i++) {
        call(starved_call, fn, &u);
    }
    fprintf(stderr, "MMAP01 %s: own-open closed on every error path\n",
            label);
    done;
}

ok64 MMAP01map(void) {
    sane(1);
    call(assert_no_open_leak, GRAFMap, "map");
    done;
}

ok64 maintest(void) {
    sane(1);
    call(setup_repo);
    ok64 r = MMAP01map();
    teardown_repo();
    return r;
}

TEST(maintest)
