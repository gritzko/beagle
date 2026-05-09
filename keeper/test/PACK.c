#include "keeper/PACK.h"
#include "keeper/ZINF.h"

#include <string.h>

#include "abc/PRO.h"
#include "abc/TEST.h"

// Helper: compress data for inflate tests (uses ZINF's zlib indirectly)
// We build compressed test data at compile time via hex literals instead.

// ---- Test 1: parse packfile header ----

ok64 PACKtest1() {
    sane(1);
    // PACK version=2 count=3
    u8 hdr[] = {
        'P', 'A', 'C', 'K',
        0, 0, 0, 2,   // version 2
        0, 0, 0, 3,   // 3 objects
    };
    u8cs from = {hdr, hdr + sizeof(hdr)};
    pack_hdr h = {};

    ok64 o = PACKDrainHdr(from, &h);
    want(o == OK);
    want(h.version == 2);
    want(h.count == 3);
    want($empty(from));

    done;
}

// ---- Test 2: bad magic ----

ok64 PACKtest2() {
    sane(1);
    u8 hdr[] = {
        'P', 'A', 'C', 'X',
        0, 0, 0, 2,
        0, 0, 0, 1,
    };
    u8cs from = {hdr, hdr + sizeof(hdr)};
    pack_hdr h = {};

    ok64 o = PACKDrainHdr(from, &h);
    want(o == PACKBADFMT);

    done;
}

// ---- Test 3: object header, commit type, small size ----

ok64 PACKtest3() {
    sane(1);
    // type=1 (commit), size=10
    // first byte: (type<<4) | (size & 0xf) = (1<<4) | 10 = 0x1a
    u8 raw[] = {0x1a};
    u8cs from = {raw, raw + sizeof(raw)};
    pack_obj obj = {};

    ok64 o = PACKDrainObjHdr(from, &obj);
    want(o == OK);
    want(obj.type == PACK_OBJ_COMMIT);
    want(obj.size == 10);

    done;
}

// ---- Test 4: object header, blob type, larger size (multi-byte varint) ----

ok64 PACKtest4() {
    sane(1);
    // type=3 (blob), size=300
    // first byte: MSB set, type=3, low4=300&0xf=12 -> (1<<7)|(3<<4)|12 = 0xbc
    // second byte: 300>>4=18, fits in 7 bits -> 18 = 0x12
    u8 raw[] = {0xbc, 0x12};
    u8cs from = {raw, raw + sizeof(raw)};
    pack_obj obj = {};

    ok64 o = PACKDrainObjHdr(from, &obj);
    want(o == OK);
    want(obj.type == PACK_OBJ_BLOB);
    want(obj.size == 300);

    done;
}

// ---- Test 5: REF_DELTA object header ----

ok64 PACKtest5() {
    sane(1);
    // type=7 (ref_delta), size=5
    // first byte: (7<<4)|5 = 0x75
    // followed by 20 bytes of base SHA1
    u8 raw[1 + 20];
    raw[0] = 0x75;
    for (int i = 0; i < 20; i++) raw[1 + i] = (u8)(0xa0 + i);

    u8cs from = {raw, raw + sizeof(raw)};
    pack_obj obj = {};

    ok64 o = PACKDrainObjHdr(from, &obj);
    want(o == OK);
    want(obj.type == PACK_OBJ_REF_DELTA);
    want(obj.size == 5);
    want($len(obj.ref_delta) == 20);
    want(*obj.ref_delta[0] == 0xa0);

    done;
}

// ---- Test 6: OFS_DELTA object header ----

ok64 PACKtest6() {
    sane(1);
    // type=6 (ofs_delta), size=1
    // first byte: (6<<4)|1 = 0x61
    // offset varint: single byte 0x0a = 10
    u8 raw[] = {0x61, 0x0a};
    u8cs from = {raw, raw + sizeof(raw)};
    pack_obj obj = {};

    ok64 o = PACKDrainObjHdr(from, &obj);
    want(o == OK);
    want(obj.type == PACK_OBJ_OFS_DELTA);
    want(obj.size == 1);
    want(obj.ofs_delta == 10);

    done;
}

// ---- Test 7: inflate zlib data ----
// zlib-compressed "Hello, packfile!\n" (17 bytes)
// Generated with: printf 'Hello, packfile!\n' | python3 -c "import sys,zlib;
//   d=zlib.compress(sys.stdin.buffer.read()); sys.stdout.buffer.write(d)"

ok64 PACKtest7() {
    sane(1);
    // We use ZINFInflate directly to test the inflate path
    con char src[] = "Hello, packfile!\n";
    u64 srclen = sizeof(src) - 1;

    // Produce compressed data via ZINFInflate's inverse isn't available,
    // so we test PACKInflate with a pre-compressed blob.
    // zlib-compress at runtime using the C API through a small helper:
    u8 zbuf[256];
    u64 zlen = 0;
    {
        // Manually call compress via ZINF's zlib link
        // Since we can't include zlib.h, use the known zlib compressed form
        // of "Hello, packfile!\n". Pre-generated:
        u8 compressed[] = {
            0x78, 0x9c, 0xf3, 0x48, 0xcd, 0xc9, 0xc9, 0xd7,
            0x51, 0x28, 0x48, 0x4c, 0xce, 0x4e, 0xcb, 0xcc,
            0x49, 0x55, 0xe4, 0x02, 0x00, 0x35, 0xe2, 0x05,
            0xab
        };
        zlen = sizeof(compressed);
        memcpy(zbuf, compressed, zlen);
    }

    u8cs from = {zbuf, zbuf + zlen};
    u8 out[64];
    u8s into = {out, out + sizeof(out)};

    ok64 o = PACKInflate(from, into, srclen);
    want(o == OK);
    want(memcmp(out, src, srclen) == 0);
    want($empty(from));

    done;
}

// ---- Test 8: malformed object header — varint with too many
// continuation bytes.  Must be rejected, not silently accepted with a
// shifted-off `size`. Regression test for the unbounded-shift UB.
ok64 PACKtest8() {
    sane(1);
    // 12 bytes, all with continuation bit set.  Type=0, size body
    // would shift past 64 bits.
    u8 raw[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
                0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
    u8cs from = {raw, raw + sizeof(raw)};
    pack_obj obj = {};

    ok64 o = PACKDrainObjHdr(from, &obj);
    want(o == PACKBADFMT);
    done;
}

// ---- Test 9: OFS_DELTA varint that would overflow u64 if accepted.
// Each continuation byte multiplies *ofs by 128 and adds; 12 bytes of
// 0xff easily overflows.
ok64 PACKtest9() {
    sane(1);
    // type=6 (ofs_delta), size=1 + huge offset varint.
    u8 raw[14];
    raw[0] = 0x61;
    for (int i = 1; i < 14; i++) raw[i] = 0xff;
    u8cs from = {raw, raw + sizeof(raw)};
    pack_obj obj = {};

    ok64 o = PACKDrainObjHdr(from, &obj);
    want(o == PACKBADFMT);
    done;
}

// ---- Test 10: object size varint truncated mid-continuation.
ok64 PACKtest10() {
    sane(1);
    u8 raw[] = {0x80};  // type=0, low4=0, continuation set, then EOF.
    u8cs from = {raw, raw + sizeof(raw)};
    pack_obj obj = {};

    ok64 o = PACKDrainObjHdr(from, &obj);
    want(o != OK);  // NODATA is fine, anything non-OK
    done;
}

ok64 maintest() {
    sane(1);
    call(PACKtest1);
    call(PACKtest2);
    call(PACKtest3);
    call(PACKtest4);
    call(PACKtest5);
    call(PACKtest6);
    call(PACKtest7);
    call(PACKtest8);
    call(PACKtest9);
    call(PACKtest10);
    done;
}

TEST(maintest)
