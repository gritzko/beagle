//  woof/test/STATIC.c — MEM-030 repro: WOOFServeStatic must release the
//  mmap (one VMA + one fd) on EVERY exit between FILEMapRO and FILEUnMap.
//
//  serve_static maps the whole asset, then feeds an envelope + the body
//  into the conn out-ring.  If the ring's idle range is smaller than the
//  body, u8bFeed returns BNOROOM; the buggy code returned from the
//  function via a plain call() BEFORE FILEUnMap, leaking one file-sized
//  mapping + fd per request (attacker-triggerable for any >ring asset).
//
//  Rather than spin up a full server with a >3 MB asset, we drive
//  WOOFServeStatic directly with a deliberately tiny out-ring so a small
//  body overflows it deterministically, and assert the process open-fd
//  count is STABLE across many BNOROOM requests (pre-fix it grew by one
//  per request).  A second case feeds the same body into a big-enough
//  ring (OK path) and also asserts fd stability — the success path was
//  already balanced, so it guards against a regression in the other
//  direction.

#include "woof/WOOF.h"

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/TEST.h"

#include <dirent.h>
#include <stdio.h>

extern woof WOOF;

//  Count entries in /proc/self/fd — the live open-fd set.  A leaked
//  FILEMapRO keeps its fd open, so this rises by one per leak.
static size_t open_fd_count(void) {
    DIR *d = opendir("/proc/self/fd");
    if (d == NULL) return 0;
    size_t n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        n++;
    }
    closedir(d);
    return n;
}

//  Count private/anonymous-or-file VMAs in /proc/self/maps.  A leaked
//  mapping adds one line; we read it as a coarse second witness.
static size_t map_line_count(void) {
    FILE *f = fopen("/proc/self/maps", "r");
    if (f == NULL) return 0;
    size_t n = 0;
    int ch;
    while ((ch = fgetc(f)) != EOF) {
        if (ch == '\n') n++;
    }
    fclose(f);
    return n;
}

//  Write `len` bytes (filled with 'A') to <dir>/.be/static/<name>.
static ok64 make_asset(path8s dir, char const *name, size_t len) {
    sane(1);
    a_path(p, dir);
    {
        a$str(dotbe,   ".be");
        a$str(staticd, "static");
        call(FILEMakeDir, $path(p));
        call(PATHu8bPush, p, dotbe);
        call(FILEMakeDir, $path(p));
        call(PATHu8bPush, p, staticd);
        call(FILEMakeDir, $path(p));
    }
    {
        a_cstr(n, name);
        call(PATHu8bPush, p, n);
    }
    int fd = FILE_CLOSED;
    call(FILECreate, &fd, $path(p));
    //  Feed the body in chunks of 'A'.
    a_carve(u8, chunk, 4096);
    for (size_t i = 0; i < 4096; i++) chunk[2][i] = 'A';
    size_t left = len;
    while (left > 0) {
        size_t take = left < 4096 ? left : 4096;
        u8c *data[2] = { chunk[2], chunk[2] + take };
        callsafe(FILEFeed(fd, (u8 const **)data), (void)FILEClose(&fd));
        left -= take;
    }
    call(FILEFlush, &fd);
    call(FILEClose, &fd);
    done;
}

//  One WOOFServeStatic invocation against `rel`, ring sized `ring_bytes`.
//  Returns the projection's ok64 in *out.
static ok64 one_serve(u8cs rel, size_t ring_bytes, ok64 *out) {
    sane(out);
    a_carve(u8, ring, ring_bytes);
    conn c = {};
    c.out[0] = ring[2];
    c.out[1] = ring[2];
    c.out[2] = ring[2];
    c.out[3] = ring[2] + ring_bytes;
    *out = WOOFServeStatic(&c, rel);
    done;
}

//  Drive WOOFServeStatic `iters` times, assert the open-fd count and the
//  map-line count are both stable (the leak signature is a monotone
//  rise).  `want` is the per-call projection result we expect.
static ok64 leak_probe(u8cs rel, size_t ring_bytes, ok64 want,
                       size_t iters) {
    sane(1);
    //  Warm one call first so any one-time allocation (FILE_WANT_BUFS,
    //  the page-cache mapping of /proc, etc.) settles before we sample.
    {
        ok64 sr = OK;
        call(one_serve, rel, ring_bytes, &sr);
        testeqv((long long)sr, (long long)want, "%lld");
    }
    size_t fd0  = open_fd_count();
    size_t map0 = map_line_count();
    for (size_t i = 0; i < iters; i++) {
        ok64 sr = OK;
        call(one_serve, rel, ring_bytes, &sr);
        testeqv((long long)sr, (long long)want, "%lld");
    }
    size_t fd1  = open_fd_count();
    size_t map1 = map_line_count();
    if (fd1 > fd0) {
        fprintf(stderr,
                "MEM-030 LEAK: open fds %zu -> %zu over %zu serves "
                "(want %lld, ring %zu)\n",
                fd0, fd1, iters, (long long)want, ring_bytes);
        fail(FILEFAIL);
    }
    if (map1 > map0) {
        fprintf(stderr,
                "MEM-030 LEAK: map lines %zu -> %zu over %zu serves\n",
                map0, map1, iters);
        fail(FILEFAIL);
    }
    done;
}

ok64 WOOFStaticLeakTest() {
    sane(1);
    call(FILEBookInit);

    //  Hermetic temp root: <tmp>/.be/static/big.bin (8 KB body).
    a_path(root, $cstr("/tmp"));
    a_cstr(tmpl, "WOOFStatic_XXXXXX");
    call(PATHu8bAddTmp, root, tmpl);

    size_t const BODY = 8192;
    call(make_asset, $path(root), "big.bin", BODY);

    //  Wire the process-wide `&HOME` root so WOOFServeStatic resolves
    //  <root>/.be/static/<rel>.  Only `HOME.root` is read by the static
    //  path; nothing else of home/WOOF is touched.
    call(PATHu8bAlloc, HOME.root);
    {
        a_dup(u8c, r, u8bData(root));
        call(PATHu8bFeed, HOME.root, r);
    }

    a$str(rel, "big.bin");

    //  1) LEAK CASE: ring (256 B) << body (8 KB) → u8bFeed BNOROOM.
    //     Pre-fix this leaked one fd + one VMA per call.
    call(leak_probe, rel, 256, BNOROOM, 64);

    //  2) OK CASE: ring (64 KB) > envelope+body → served, balanced.
    call(leak_probe, rel, 1UL << 16, OK, 64);

    //  cleanup
    PATHu8bFree(HOME.root);
    call(FILERmDir, $path(root), true);
    done;
}

TEST(WOOFStaticLeakTest);
