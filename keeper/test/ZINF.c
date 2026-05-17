//  ZINF test: deflate then inflate a chain of versions
//
//  Simulates packing ~20 small object versions back-to-back:
//  deflate each into a contiguous buffer, then inflate them
//  one by one, verifying round-trip fidelity.

#include "dog/git/ZINF.h"

#include <string.h>

#include "abc/PRO.h"
#include "abc/TEST.h"

#define NVER 20
#define VERSZ 256

static void make_version(u8 *buf, u64 *len, int idx) {
    int n = snprintf((char *)buf, VERSZ, "version %02d: ", idx);
    for (int i = n; i < VERSZ - 1; i++)
        buf[i] = (u8)('A' + ((idx + i) % 26));
    buf[VERSZ - 1] = '\n';
    *len = VERSZ;
}

ok64 ZINFtest1() {
    sane(1);

    a_pad(u8, pack, NVER * VERSZ * 2);
    u64 offsets[NVER];
    u64 csizes[NVER];

    // --- deflate chain ---
    for (int i = 0; i < NVER; i++) {
        a_pad(u8, ver, VERSZ);
        u64 vlen = 0;
        make_version(u8bHead(ver), &vlen, i);
        u8bFed(ver, vlen);

        offsets[i] = u8bDataLen(pack);
        u64 idle_before = u8bIdleLen(pack);
        a_dup(u8c, src, u8bDataC(ver));
        ok64 o = ZINFDeflate(u8bIdle(pack), src);
        want(o == OK);
        u64 produced = idle_before - u8bIdleLen(pack);
        want(produced > 0);
        csizes[i] = produced;
        u8bFed(pack, produced);
    }

    // --- inflate chain and verify ---
    for (int i = 0; i < NVER; i++) {
        a_pad(u8, orig, VERSZ);
        u64 olen = 0;
        make_version(u8bHead(orig), &olen, i);

        a_pad(u8, got, VERSZ);
        u64 idle_before = u8bIdleLen(got);

        a_dup(u8c, data, u8bDataC(pack));
        u8csUsed(data, offsets[i]);
        u64 src_before = u8csLen(data);

        ok64 o = ZINFInflate(u8bIdle(got), data);
        want(o == OK);
        u64 produced = idle_before - u8bIdleLen(got);
        u64 consumed = src_before - u8csLen(data);
        want(produced == olen);
        want(consumed == csizes[i]);
        want(memcmp(u8bHead(orig), u8bHead(got), olen) == 0);
    }

    done;
}

ok64 maintest() {
    sane(1);
    call(ZINFtest1);
    done;
}

TEST(maintest)
