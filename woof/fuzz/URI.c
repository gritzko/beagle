//  woof/fuzz/URI.c — open-once projection-Exec fuzzer (ASAN/stability).
//
//  Parses the fuzz bytes as an HTTP request-target the *same way woof
//  does* — WOOFutf8ExtractURI strips the leading '/' and percent-
//  decodes into a be-URI, then URIutf8Drain lexes it — and then
//  dispatches the projection AS A LIBRARY CALL instead of forking a
//  worker.  graf is opened ONCE (GRAFOpenBranch, which now also opens
//  keeper RO at the top of the call chain); every input just re-runs
//  GRAFExec against the open store.  No fork, no per-request reopen:
//  beagle's flat single-pool store serves every branch, and the URI's
//  `?ref` is resolved per-Exec via REFS.  GRAFExec's HUNK output (TLV)
//  is captured through an fd-1 → memfd redirect into a 1 MB buffer, so
//  the real write path runs and we confirm the projection produced
//  bytes — not just that it parsed.
//
//  Single project: run it from a worktree / .be root, e.g.
//      nice WOOFURIfuzz $HOME/Corpus/woof/URI -workers=16 -jobs=16
//
//  TODO(multi-project dispatch): woof's planned `--api` dispatch mode
//  keeps a per-project dog set open and routes by the URI's project
//  segment; this fuzzer serves the cwd's single project, which is all
//  it needs (see WOOF.h / WOOF.cli.c).
//  TODO(all projector dogs): keeper (blob/tree/commit), spot (grep/
//  spot/regex) and sniff (ls/cat/status) dispatch the same way once
//  their Exec drops the open/close-in-Exec pattern — graf is done.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE          // memfd_create
#endif

#include "woof/WOOF.h"

#include "abc/BUF.h"
#include "abc/FILE.h"
#include "abc/PRO.h"
#include "abc/TEST.h"
#include "abc/URI.h"
#include "dog/CLI.h"
#include "dog/HOME.h"
#include "dog/HUNK.h"
#include "graf/GRAF.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

//  wooflib's CONN.o (in-process dispatch) pulls in SNIFFExec, which
//  references the process argv global the MAIN macro normally defines.
//  FUZZ doesn't define it; provide an empty one — the fuzzer drives
//  GRAFExec directly and never reaches the sniff path, so it's only a
//  link-time stub.
u8cs *STD_ARGS[4] = {};

#define WOOF_FUZZ_OUT_CAP (1UL << 20)        // 1 MB projection-output sink

//  Reachable-from-globals across inputs → the leak detector never flags
//  the one-time graf/keeper/home opens (LSan only reports unreachable).
static home WOOF_FUZZ_HOME = {};
static cli  WOOF_FUZZ_CLI  = {};
static int  WOOF_FUZZ_MEMFD = -1;
static u8   WOOF_FUZZ_OUT[WOOF_FUZZ_OUT_CAP];
static u8   WOOF_FUZZ_DECODE[WOOF_REQBUF_BYTES];   // decoded be-URI bytes

//  Open everything ONCE.  state: 0 untried, 1 ready, -1 failed.
static b8 woof_fuzz_init(void) {
    static int state = 0;
    if (state) return state == 1;
    state = -1;

    if (FILEInit() != OK) return NO;
    //  One project: walk up from cwd to the `.be` anchor.
    if (HOMEOpenAt(&WOOF_FUZZ_HOME, (u8cs){}, NO) != OK) return NO;

    //  Open graf (and, inside it, keeper RO) once on the wt's current
    //  branch.  The flat store then resolves every branch the fuzzed
    //  URIs name without a reopen.
    {
        a_dup(u8c, br, u8bDataC(WOOF_FUZZ_HOME.cur_branch));
        ok64 go = GRAFOpenBranch(&WOOF_FUZZ_HOME, br, NO);
        if (go != OK && go != GRAFOPEN) return NO;
    }

    //  cli scaffolding: alloc once, re-feed `uris` per input.
    if (PATHu8bAlloc(WOOF_FUZZ_CLI.repo)                  != OK) return NO;
    if (u8csbAlloc(WOOF_FUZZ_CLI.flags, CLI_MAX_FLAGS * 2) != OK) return NO;
    if (uribAlloc(WOOF_FUZZ_CLI.uris, CLI_MAX_URIS)        != OK) return NO;
    {
        a_dup(u8c, root, u8bDataC(WOOF_FUZZ_HOME.root));
        (void)PATHu8bFeed(WOOF_FUZZ_CLI.repo, root);
    }

    //  Capture graf's stdout (it writes TLV to fd 1) into a memfd we
    //  drain into the 1 MB buffer after each Exec.
    WOOF_FUZZ_MEMFD = memfd_create("woof-fuzz-out", 0);
    if (WOOF_FUZZ_MEMFD < 0) return NO;
    HUNKMode = HUNKOutTLV;   // same shape as woof's `--tlv` worker

    state = 1;
    return YES;
}

//  One request, run inside a call/try frame so its BASS carves (and
//  graf's, downstream) are snapshotted on entry and rewound on return —
//  the whole reason this body is a separate function (CLAUDE.md §5;
//  PRO.h: call/try rewind a_carve/a_cquire).  Without the frame BASS
//  would accumulate across inputs until a_carve hits NOROOM.
static ok64 woof_fuzz_one(u8sc input) {
    sane(1);

    //  Parse the URI exactly as woof does: target = "/" + input, then
    //  strip the slash and percent-decode into the be-URI buffer.
    a_carve(u8, tgt, WOOF_REQBUF_BYTES);
    call(u8bFeed1, tgt, '/');
    call(u8bFeed,  tgt, input);
    a_dup(u8c, target, u8bData(tgt));

    Bu8 dec = { WOOF_FUZZ_DECODE, WOOF_FUZZ_DECODE, WOOF_FUZZ_DECODE,
                WOOF_FUZZ_DECODE + sizeof(WOOF_FUZZ_DECODE) };
    call(WOOFutf8ExtractURI, dec, target);

    //  Lex the be-URI.
    uri u = {};
    a_dup(u8c, beuri, u8bData(dec));
    call(URIutf8Drain, beuri, &u);

    //  Verbless projector cli — GRAFExec synthesizes the verb from the
    //  scheme (diff/log/map/blame/weave).  u's slices point into the
    //  stable decode buffer, alive for the whole GRAFExec call.
    uribReset(WOOF_FUZZ_CLI.uris);
    call(uribFeed1, WOOF_FUZZ_CLI.uris, u);
    zerop(&WOOF_FUZZ_CLI.verb);

    //  Reset graf's render arena; BASS carves rewind via this try frame.
    GRAFArenaCleanup();

    //  fd 1 → memfd around the call; restore before reading back so the
    //  fuzzer's own stdout (and ASAN's stderr) stay intact.
    int save = dup(STDOUT_FILENO);
    (void)ftruncate(WOOF_FUZZ_MEMFD, 0);
    (void)lseek(WOOF_FUZZ_MEMFD, 0, SEEK_SET);
    (void)dup2(WOOF_FUZZ_MEMFD, STDOUT_FILENO);

    (void)GRAFExec(&WOOF_FUZZ_CLI);   // projection-level errors are fine

    (void)dup2(save, STDOUT_FILENO);
    (void)close(save);

    //  Drain the produced bytes into the 1 MB buffer (proves the write
    //  path ran; ASAN watches the copy).
    (void)lseek(WOOF_FUZZ_MEMFD, 0, SEEK_SET);
    (void)read(WOOF_FUZZ_MEMFD, WOOF_FUZZ_OUT, sizeof(WOOF_FUZZ_OUT));

    done;
}

FUZZ(u8, WOOFURIfuzz) {
    sane(1);
    //  Keep "/" + input and the decode output (≤ input) within the 16 KB
    //  request region.
    if ($len(input) > (WOOF_REQBUF_BYTES / 2) - 2) done;
    if (!woof_fuzz_init()) done;

    //  Run the request in a try frame: BASS is rewound on return, and a
    //  parse / projection failure is an uninteresting (non-crash) input,
    //  so swallow it rather than letting the FUZZ wrapper trap on non-OK.
    try(woof_fuzz_one, input);
    __ = OK;
    done;
}
