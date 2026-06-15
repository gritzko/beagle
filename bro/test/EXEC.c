//  EXEC — BROExec resource-safety tests (MEM-020).
//
//  Cases:
//    1. Arena overflow at the per-URI uri-host: the file is mapped
//       (FILEMapRO) at BRO.exe.c before the arena copy of the URI.
//       If that copy hits BNOROOM the mapping must STILL be tracked so
//       BROClose releases it — otherwise the mmap (and its booked fd
//       slot) leaks.  We swap in a tiny staging arena so the uri-host
//       overflows, run BROExec, close, then assert no booked FILE slot
//       stays mapped.
//
//  No terminal, no rendering: the failing arena copy aborts staging
//  before any hunk reaches the pager.

#include "BRO.h"

#include <string.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/TEST.h"
#include "abc/URI.h"
#include "dog/CLI.h"
#include "dog/DOG.h"

//  Count booked FILE maps still live (a non-NULL FILE_WANT_BUFS slot
//  means an open+mapped fd).  bro's own arena/hunks/toks/maps buffers
//  are anonymous u8bMap mmaps — they never touch FILE_WANT_BUFS, so
//  any live slot here is a leaked file mapping.
static u32 booked_maps_live(void) {
    if (FILE_WANT_BUFS == NULL) return 0;
    u32 n = 0;
    for (int fd = 0; fd < FILE_MAX_OPEN; fd++) {
        u8bp slot = FILE_WANT_BUFS[fd];
        if (slot && slot[0]) n++;
    }
    return n;
}

//  Write `content` to `path` (create+map+unmap leaves it on disk).
static ok64 spit_file(char const *path, char const *content) {
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

//  Build a one-URI cli for `arg` exactly as CLIParse does: store the
//  raw arg text (decomposed on demand by CLIUriAt downstream).
static void cli_one_uri(cli *c, u8csc arg) {
    u8cs raw = {arg[0], arg[1]};
    (void)u8csbFeed1(c->uris, raw);
}

//  MEM-020 repro: arena overflow at the per-URI uri-host must not leak
//  the file mapping done a few lines earlier.
ok64 EXECtest_arena_overflow_no_map_leak(void) {
    sane(1);
    call(FILEInit);

    char const *path = "/tmp/bro-exec-mem020.c";
    a_cstr(argc_lit, "/tmp/bro-exec-mem020.c");
    u8cs arg = {argc_lit[0], argc_lit[1]};

    call(spit_file, path, "int main(void){return 0;}\n");

    home h = {};        // local-file path never touches the keeper / h
    bro b = {};
    try(BROOpen, &b, NO);

    //  Swap the 128MB staging arena for a tiny one so the per-URI
    //  u8bHost copy of the URI (BRO.exe.c, run AFTER FILEMapRO) cannot
    //  fit and returns BNOROOM.  BROExec's BROArenaInit does u8bShedAll,
    //  which makes IDLE span the whole (tiny) buffer — so it stays tiny.
    then if (b.arena[0]) u8bUnMap(b.arena);
    then try(u8bMap, b.arena, $len(arg) > 1 ? $len(arg) - 1 : 1);

    cli c = {};
    then try(PATHu8bAlloc, c.repo);
    then try(u8csbAlloc, c.uris, CLI_MAX_URIS);

    u32 before = booked_maps_live();
    if (__ == OK) {
        cli_one_uri(&c, arg);
        //  BROExec maps the file (FILEMapRO) then overflows the tiny
        //  arena copying the URI.  The overflow is non-fatal — the
        //  point under test is that the mapping is REGISTERED for
        //  teardown, so BROClose releases it.  Pre-fix the map was
        //  never recorded and leaked past BROClose (MEM-020).
        (void)BROExec(&b, &c);
    }

    u8csbFree(c.uris);
    PATHu8bFree(c.repo);
    BROClose(&b);

    //  Every FILE map opened by BROExec must be gone after BROClose.
    testeq(booked_maps_live(), before);

    int _ = unlink(path);
    (void)_;
    done;
}

ok64 EXECtest(void) {
    sane(1);
    call(EXECtest_arena_overflow_no_map_leak);
    done;
}

TEST(EXECtest)
