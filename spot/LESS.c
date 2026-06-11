#include "LESS.h"

#include "abc/FILE.h"
#include "abc/PRO.h"
#include "dog/HUNK.h"

// --- Producer-side staging state ---
Bu8      less_arena = {};
LESShunk less_hunks[LESS_MAX_HUNKS];
u8bp     less_maps[LESS_MAX_MAPS];
Bu32     less_toks[LESS_MAX_MAPS];
u32      less_nhunks = 0;
u32      less_nmaps  = 0;

int spot_out_fd = -1;

ok64 LESSArenaInit(void) {
    less_nhunks = 0;
    less_nmaps  = 0;
    zero(less_hunks);
    zero(less_maps);
    zero(less_toks);
    if (!BNULL(less_arena)) {
        u8bShedAll(less_arena);  // empty DATA, IDLE spans full buffer
        return OK;
    }
    return u8bMap(less_arena, LESS_ARENA_SIZE);
}

void LESSArenaCleanup(void) {
    for (u32 i = 0; i < less_nmaps; i++) {
        if (less_toks[i][0] != NULL) u32bUnMap(less_toks[i]);
        if (less_maps[i] != NULL)    FILEUnMap(less_maps[i]);
    }
    less_nhunks = 0;
    less_nmaps  = 0;
}

//  Take ownership of `mapped` and `toks` (4-pointer buffer
//  descriptors).  Caller must not use them after the call —
//  buffers are single-owner; LESS will unmap them at cleanup.
void LESSDefer(u8bp mapped, u32bp toks) {
    if (less_nmaps >= LESS_MAX_MAPS) {
        // Table full: cannot record the descriptors, so cleanup could
        // never reclaim them — unmap here instead of leaking. `toks`
        // may be an empty (Bu32){} descriptor from the grep callbacks.
        if (mapped != NULL) FILEUnMap(mapped);
        if (toks[0] != NULL) u32bUnMap(toks);
        return;
    }
    less_maps[less_nmaps] = mapped;
    for (int i = 0; i < 4; i++) less_toks[less_nmaps][i] = toks[i];
    less_nmaps++;
}

// Serialize the just-built hunk via HUNKu8sFeedOut (dispatched off
// the module-global `HUNKMode`), write to spot_out_fd, then rewind
// the entire arena.  After emission the hunk's title, toks and hili
// slices (which live in the arena) are dead — the pipe owns the
// bytes now.  Full rewind keeps the arena from filling up across
// hundreds of streaming hunks.
ok64 LESSHunkEmit(void) {
    sane(YES);
    if (spot_out_fd < 0) {
        // No output set up yet — accumulate nhunks for LESSRun.
        less_nhunks++;
        done;
    }
    LESShunk *hk = &less_hunks[less_nhunks];

    // Serialization goes into arena idle space (past title/toks/hili).
    u8cp start = u8bIdleHead(less_arena);
    //  Serialize, then drain the serialized bytes to the output fd.
    //  Both steps are fallible (HUNKu8sFeedOut can run out of arena
    //  idle space; FILEFeedAll fails on EPIPE/EAGAIN/short write).  Run
    //  them under `try()` so a failure does not skip the arena rewind:
    //  the hunk's title/toks/hili plus any partial TLV must be dropped
    //  on both paths or the arena fills up across streaming hunks.
    try(HUNKu8sFeedOut, u8bIdle(less_arena), hk);
    then {
        u8cs ser = {start, u8bIdleHead(less_arena)};
        //  FILEFeedAll loops on EINTR and advances by the write count;
        //  any other short/failed write (EAGAIN, EPIPE) returns
        //  FILEFAIL so a broken output stream propagates.
        try(FILEFeedAll, spot_out_fd, ser);
    }

    // Full arena rewind: title, toks, hili, and serialized bytes are
    // all consumed (or dropped on failure).  The hunk struct in
    // less_hunks[0] will be reused next time CAPOBuildHunk runs.
    u8bShedAll(less_arena);
    done;
}

ok64 LESSRun(LESShunk const *hunks, u32 nhunks) {
    sane(1);
    (void)hunks;
    (void)nhunks;
    LESSArenaCleanup();
    done;
}
