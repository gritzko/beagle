#include "dog/git/PACK.h"
#include "dog/git/ZINF.h"

#include <string.h>

#include "abc/B.h"
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

// ---- Test 11: PACKu8sFeedObjHdr ⇄ PACKDrainObjHdr round-trip.
// The encode counterpart of the object-header varint must reproduce
// exactly what the decoder reads back, across the 4-bit boundary, the
// 7-bit continuation groups, and every raw object type (CODE-004/005:
// relocated here from keeper's pack writers).
ok64 PACKtest11() {
    sane(1);
    u64 sizes[] = {0, 1, 15, 16, 127, 128, 4095, 4096, 1234567,
                   (1ULL << 34) + 7};
    u8  types[] = {PACK_OBJ_COMMIT, PACK_OBJ_TREE, PACK_OBJ_BLOB,
                   PACK_OBJ_TAG};
    for (size_t t = 0; t < sizeof(types) / sizeof(types[0]); t++) {
        for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
            a_pad(u8, buf, 32);
            PACKu8sFeedObjHdr(buf, types[t], sizes[i]);
            u8cs scan = {u8bDataHead(buf), u8bIdleHead(buf)};
            pack_obj obj = {};
            call(PACKDrainObjHdr, scan, &obj);
            want(obj.type == types[t]);
            want(obj.size == sizes[i]);
            want($empty(scan));   //  header consumed exactly
        }
    }
    done;
}

// ---- Test 12: PACKu8sFeedObjHdr propagates SNOROOM on a full buffer.
// A buffer too small for the whole type/size varint must refuse rather
// than emit a truncated header into the pack stream (can-fail-but-was-
// void regression: the encoder now returns the BUF overflow code).
ok64 PACKtest12() {
    sane(1);
    //  size=16 needs the low-4 byte plus one 7-bit continuation byte:
    //  a 1-byte buffer can hold the first byte but not the second, so
    //  the second u8bFeed1 must surface SNOROOM.
    a_pad(u8, tiny, 1);
    want(PACKu8sFeedObjHdr(tiny, PACK_OBJ_BLOB, 16) == SNOROOM);

    //  Zero-capacity buffer fails on the very first byte.
    a_pad(u8, none, 0);
    want(PACKu8sFeedObjHdr(none, PACK_OBJ_COMMIT, 0) == SNOROOM);

    //  A right-sized buffer still succeeds (no false positive).
    a_pad(u8, ok, 8);
    want(PACKu8sFeedObjHdr(ok, PACK_OBJ_BLOB, 16) == OK);
    done;
}

// ---- Test 13: PACKu8sFeedOfs ⇄ OFS decode round-trip.
// The OFS_DELTA negative-offset varint encoder must be the exact
// inverse of the decoder inside PACKDrainObjHdr, across every 7-bit
// continuation-group boundary and into large u64 offsets (GIT-002:
// the two hand-rolled copies in keeper/KEEP.c + js/pack.hpp are
// replaced by this one shared encoder; this pins the wire encoding).
ok64 PACKtest13() {
    sane(1);
    u64 vals[] = {0, 1, 0x7f, 0x80, 0x81, 0x3fff, 0x4000, 0x4001,
                  0x1fffff, 0x200000, 1234567, (1ULL << 35) + 13,
                  UINT32_MAX, ((u64)1 << 50) + 1};
    for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
        //  Encode the offset behind an OFS_DELTA object header (size=1)
        //  so PACKDrainObjHdr's OFS branch decodes it back: the header
        //  byte is (PACK_OBJ_OFS_DELTA<<4)|1.
        a_pad(u8, buf, 32);
        call(PACKu8sFeedObjHdr, buf, PACK_OBJ_OFS_DELTA, 1);
        call(PACKu8sFeedOfs, buf, vals[i]);
        u8cs scan = {u8bDataHead(buf), u8bIdleHead(buf)};
        pack_obj obj = {};
        call(PACKDrainObjHdr, scan, &obj);
        want(obj.type == PACK_OBJ_OFS_DELTA);
        want(obj.ofs_delta == vals[i]);
        want($empty(scan));   //  offset varint consumed exactly
    }
    done;
}

// ---- Test 14: PACKu8sFeedOfs propagates SNOROOM on a full buffer.
// A buffer too small for the whole offset varint must refuse rather
// than emit a truncated offset into the pack stream.
ok64 PACKtest14() {
    sane(1);
    //  0x80 needs two bytes (one continuation group): a 1-byte buffer
    //  holds the first emitted byte but not the second, so the encoder
    //  must surface SNOROOM rather than truncate.
    a_pad(u8, tiny, 1);
    want(PACKu8sFeedOfs(tiny, 0x80) == SNOROOM);

    //  Zero-capacity buffer fails on the very first byte.
    a_pad(u8, none, 0);
    want(PACKu8sFeedOfs(none, 0) == SNOROOM);

    //  A right-sized buffer still succeeds (no false positive).
    a_pad(u8, ok, 8);
    want(PACKu8sFeedOfs(ok, 0x80) == OK);
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
    call(PACKtest11);
    call(PACKtest12);
    call(PACKtest13);
    call(PACKtest14);
    done;
}

TEST(maintest)
