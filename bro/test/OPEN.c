// OPEN — BROOpenFile resource-safety tests (MEM-045).
//
// BROOpenFile (BRO.c) FILEMapRO's a file into a booked FILE_WANT_BUFS
// slot and stores it in st->files[idx].mapped, but only commits the
// slot (st->nsaves++) at the very END.  Two fallible call()s run in
// between:
//   - call(u8bHost, bro_arena, fv->hunk.uri, rp)   (URI copy)
//   - call(BROBuildIndex, st)                       (line index)
// If either fails, BROOpenFile early-returns with the slot uncommitted
// and the booked map STILL referenced by st->files[idx].mapped — which
// nobody frees (BROBack only frees committed slots, BROClose only frees
// fds recorded via BRODefer, and BROOpenFile records none).  The mmap +
// its booked fd slot leak.  This leak is NOT LSan-tracked (the mapping
// is reachable via the still-live, never-torn-down st), so we detect it
// by counting live booked FILE maps across many failing iterations and
// asserting the count stays stable.
//
// The test includes BRO.c directly (like SEARCH.c) to reach the
// file-static BROOpenFile / BROstate / bro_state.

#include "../BRO.c"

#include <string.h>
#include <unistd.h>

#include "abc/TEST.h"

//  Count booked FILE maps still live (a non-NULL FILE_WANT_BUFS slot
//  with a live buffer means an open+mapped fd).  bro's own
//  arena/hunks/toks/maps buffers are anonymous u8bMap mmaps — they
//  never touch FILE_WANT_BUFS, so any live slot here is a file mapping.
static u32 open_booked_maps_live(void) {
    if (FILE_WANT_BUFS == NULL) return 0;
    u32 n = 0;
    for (int fd = 0; fd < FILE_MAX_OPEN; fd++) {
        u8bp slot = FILE_WANT_BUFS[fd];
        if (slot && slot[0]) n++;
    }
    return n;
}

//  Write `content` to `path` (create+map+unmap leaves it on disk).
static ok64 open_spit_file(char const *path, char const *content) {
    sane(path && content);
    a_path(p);
    a_cstr(pc, path);
    call(PATHu8bFeed, p, pc);
    size_t n = strlen(content);
    u8bp mapped = NULL;
    call(FILEMapCreate, &mapped, $path(p), n);
    a_cstr(cc, content);
    u8bFeed(mapped, cc);
    FILEUnMap(mapped);
    done;
}

//  MEM-045 repro: BROOpenFile maps the file, then the URI-copy
//  call(u8bHost,...) overflows the (shrunk) arena and early-returns.
//  Pre-fix the booked map referenced by the uncommitted st->files slot
//  leaks; each iteration burns one more fd + mapping.  We assert the
//  live booked-map count is stable across N iterations.
ok64 OPENtest_open_arena_overflow_no_map_leak(void) {
    sane(1);
    call(FILEInit);

    char const *dir  = "/tmp";
    char const *name = "bro-open-mem045.c";
    char const *full = "/tmp/bro-open-mem045.c";
    call(open_spit_file, full, "int main(void){return 0;}\n");

    home h = {};        // local-file path never touches the keeper / h
    bro b = {};
    try(BROOpen, &b, NO);

    //  Swap the 128MB arena for a 1-byte one so the per-open u8bHost
    //  copy of the URI (BROOpenFile, run AFTER FILEMapRO) cannot fit and
    //  returns BNOROOM — exercising the first intervening call().
    then if (b.arena[0]) u8bUnMap(b.arena);
    then try(u8bMap, b.arena, 1);

    if (__ != OK) { BROClose(&b); int _ = unlink(full); (void)_; done; }

    //  Drive BROOpenFile many times with a state that fails the URI
    //  copy.  A fresh BROstate per iteration (nsaves=0) means the slot
    //  is never committed; the only thing keeping the count flat is the
    //  fix freeing the booked map on the error branch.
    u8cs rel = u8slit("bro-open-mem045.c");

    u32 before = 0;
    enum { ITERS = 64 };
    for (int i = 0; i < ITERS; i++) {
        BROstate st = {};
        st.cols = 80;
        st.rows = 24;
        st.tty_fd = -1;
        ok64 o = BROOpenFile(&st, rel, dir, 0);
        //  The URI overflow must make this fail; if it ever returns OK
        //  the repro stopped reproducing (arena too big) — flag it.
        if (o == OK) {
            //  Committed a real view: tear it down so we don't leak in
            //  the test itself, and fail loudly.
            while (st.nsaves > 0) BROBack(&st);
            BROClose(&b);
            int _ = unlink(full); (void)_;
            fail(FAILSANITY);
        }
        //  Sample the live count once the machinery has warmed up (first
        //  iteration may map+unmap transient slots).
        if (i == 0) before = open_booked_maps_live();
        else testeq(open_booked_maps_live(), before);
    }

    BROClose(&b);

    //  After teardown no booked map opened by BROOpenFile may survive.
    testeq(open_booked_maps_live(), 0u);

    int _ = unlink(full); (void)_;
    (void)name;
    done;
}

ok64 OPENtest(void) {
    sane(1);
    call(OPENtest_open_arena_overflow_no_map_leak);
    done;
}

TEST(OPENtest)
