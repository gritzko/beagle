//  BOOKMARK — pack bookmark index primitives.
//
//  Confirms that a pack-type index entry round-trips through
//  wh64Pack / keepPackBmVal / the key + val extractors, and that
//  bookmarks sort by (file_id, offset).

#include "keeper/KEEP.h"

#include "abc/PRO.h"
#include "abc/TEST.h"

//  Pack bookmark layout (LOG.md / WIRE.md Phase 0):
//    key = wh64Pack(KEEP_TYPE_PACK, file_id, offset)
//    val = keepPackBmVal(obj_count32, byte_len32)
ok64 BOOKMARKtest1() {
    sane(1);
    u32 file_id = 42;
    u64 offset  = 0x1234567890ULL & WHIFF_OFF_MASK;
    u32 count   = 0x01020304;
    u32 length  = 0xdeadbeef;

    wh128 e = {
        .key = wh64Pack(KEEP_TYPE_PACK, file_id, offset),
        .val = keepPackBmVal(count, length),
    };

    want(wh64Type(e.key)         == KEEP_TYPE_PACK);
    want(wh64Id(e.key)           == file_id);
    want(wh64Off(e.key)          == offset);
    want(keepPackBmCount(e.val)  == count);
    want(keepPackBmLen(e.val)    == length);

    //  Edge cases: zero count, max u32 length.
    want(keepPackBmCount(keepPackBmVal(0, 0)) == 0);
    want(keepPackBmLen(keepPackBmVal(0, 0))   == 0);
    want(keepPackBmCount(keepPackBmVal(0xFFFFFFFFu, 1)) == 0xFFFFFFFFu);
    want(keepPackBmLen(keepPackBmVal(1, 0xFFFFFFFFu))   == 0xFFFFFFFFu);

    done;
}

//  Pack bookmarks for two packs in the same file at different
//  offsets must sort by offset ascending.
ok64 BOOKMARKtest2() {
    sane(1);
    wh128 a = {
        .key = wh64Pack(KEEP_TYPE_PACK, 1, 12),    // first pack
        .val = keepPackBmVal(7, 1024),
    };
    wh128 b = {
        .key = wh64Pack(KEEP_TYPE_PACK, 1, 4096),  // later pack
        .val = keepPackBmVal(3, 8192),
    };
    want(wh128Z(&a, &b));
    done;
}

//  Object lookups restrict type to 1..4.  PACK bookmarks use 0xF,
//  so they are always above that range and never returned for an
//  object query.
ok64 BOOKMARKtest3() {
    sane(1);
    u64 hashlet = 0x0aaaabbbbccccdddULL & WHIFF_HASHLET60_MASK;
    u64 obj_hi = keepKeyPack(KEEP_OBJ_TAG, hashlet);
    u64 pack_k = wh64Pack(KEEP_TYPE_PACK, 1, 0);  // any PACK entry
    //  Any PACK key has type nibble 0xF in LS bits, so pack_k's
    //  low 4 bits ≥ obj_hi's low 4 bits (=4).  Regardless of
    //  hashlet/file_id/offset, pack_k & 0xF > obj_hi & 0xF.
    want((pack_k & 0xF) > (obj_hi & 0xF));
    done;
}

ok64 maintest() {
    sane(1);
    call(BOOKMARKtest1);
    call(BOOKMARKtest2);
    call(BOOKMARKtest3);
    done;
}

TEST(maintest)
