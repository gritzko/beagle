//  PACKRESOLVE: table-driven property test for the GIT-004 shared
//  dog/git OFS-only resolver (PACKResolveOfs).
//
//  The resolver is the pure byte+offset delta chase extracted from
//  keeper's native get (and reused by the js/JABC binding).  This test
//  drives it directly with synthetic OFS-only packs:
//
//    1. round-trip: a valid raw→OFS_DELTA→OFS_DELTA chain resolves
//       every object back to its original bytes (delta-of-delta);
//    2. corruption bounds (must SURVIVE the simplification, GIT-004 §5):
//         - self-ref   : ofs_delta == 0  → PACKFAIL (no spin);
//         - underflow0 : ofs_delta == cur → cur underflows to 0 → PACKFAIL;
//         - past-tail  : start offset past the pack tail → PACKFAIL;
//    3. REF backstop: a REF_DELTA record → PACKREF (bounded, loud),
//       never a silent success/empty (GIT-004 assert-guarded dead arm).

#include "git/PACK.h"

#include <stdio.h>
#include <string.h>

#include "abc/B.h"
#include "abc/PRO.h"
#include "abc/S.h"
#include "abc/TEST.h"
#include "git/DELT.h"
#include "git/ZINF.h"

#define SCRATCH (1u << 20)

//  Append one raw object record (header + deflated content) to `log`,
//  returning its start offset via *off_out.
static ok64 feed_raw(u8bp log, u8 type, u8csc content, u64 *off_out) {
    sane(u8bOK(log));
    *off_out = (u64)u8bDataLen(log);
    a_pad(u8, hdr, 16);
    call(PACKu8sFeedObjHdr, hdr, type, u8csLen(content));
    a_dup(u8c, hb, u8bData(hdr));
    call(u8bFeed, log, hb);
    a_dup(u8c, cz, content);
    call(ZINFDeflate, u8bIdle(log), cz);
    done;
}

//  Append one OFS_DELTA record (delta of `target` against `base`) to
//  `log`, back-pointing to `base_off`.  Returns the record offset.
static ok64 feed_ofs(u8bp log, u8csc base, u8csc target, u64 base_off,
                     u8bp dscratch, u64 *off_out) {
    sane(u8bOK(log));
    u64 cur = (u64)u8bDataLen(log);
    *off_out = cur;
    u8bReset(dscratch);
    call(DELTEncode, base, target, dscratch);
    u64 dlen = (u64)u8bDataLen(dscratch);
    a_pad(u8, hdr, 16);
    call(PACKu8sFeedObjHdr, hdr, PACK_OBJ_OFS_DELTA, dlen);
    a_dup(u8c, hb, u8bData(hdr));
    call(u8bFeed, log, hb);
    a_pad(u8, ofs, 16);
    call(PACKu8sFeedOfs, ofs, cur - base_off);
    a_dup(u8c, ob, u8bData(ofs));
    call(u8bFeed, log, ob);
    a_dup(u8c, dz, u8bDataC(dscratch));
    call(ZINFDeflate, u8bIdle(log), dz);
    done;
}

//  Resolve `off` in `log` and compare to `want`.
static ok64 expect_resolve(u8cs log, u64 off, u8csc want) {
    sane($ok(log));
    a_carve(u8, bb, SCRATCH);
    a_carve(u8, db, SCRATCH);
    u8s bsc = {u8bHead(bb), u8bTerm(bb)};
    u8s dsc = {u8bHead(db), u8bTerm(db)};
    u8cs body = {};
    u8 type = 0;
    call(PACKResolveOfs, log, off, bsc, dsc, body, &type);
    if (type != PACK_OBJ_BLOB) {
        fprintf(stderr, "resolve: type=%u want blob\n", type);
        fail(TESTFAIL);
    }
    if ((u64)u8csLen(body) != (u64)u8csLen(want) ||
        memcmp(body[0], want[0], u8csLen(want)) != 0) {
        fprintf(stderr, "resolve @%llu: %llu bytes, want %llu\n",
                (unsigned long long)off, (unsigned long long)u8csLen(body),
                (unsigned long long)u8csLen(want));
        fail(TESTFAIL);
    }
    done;
}

//  Resolve `off` in `log` and assert a specific bounded error code.
static ok64 expect_fail(u8cs log, u64 off, ok64 want) {
    sane($ok(log));
    a_carve(u8, bb, SCRATCH);
    a_carve(u8, db, SCRATCH);
    u8s bsc = {u8bHead(bb), u8bTerm(bb)};
    u8s dsc = {u8bHead(db), u8bTerm(db)};
    u8cs body = {};
    u8 type = 0;
    ok64 got = PACKResolveOfs(log, off, bsc, dsc, body, &type);
    if (got != want) {
        fprintf(stderr, "resolve @%llu: got %llx want %llx\n",
                (unsigned long long)off, (unsigned long long)got,
                (unsigned long long)want);
        fail(TESTFAIL);
    }
    done;
}

//  (1) Valid raw→OFS→OFS chain: each version resolves to its bytes.
static ok64 round_trip() {
    sane(1);
    a_cstr(v0, "the quick brown fox jumps over the lazy dog, take 0");
    a_cstr(v1, "the quick brown fox jumps over the lazy dog, take 1!!");
    a_cstr(v2, "the quick brown fox jumps over the lazy dog, take 22?");

    a_carve(u8, logb_c, SCRATCH);
    zerob(logb_c);
    u8bp log = (u8bp)logb_c;  //  used as a growable buffer
    u8bReset(log);
    a_carve(u8, ds, SCRATCH);
    u8bp dscratch = (u8bp)ds;

    u8s into = {u8bIdleHead(log), u8bTerm(log)};
    call(PACKu8sFeedHdr, into, 3);
    u8bFed(log, 12);

    u64 o0 = 0, o1 = 0, o2 = 0;
    call(feed_raw, log, PACK_OBJ_BLOB, v0, &o0);
    call(feed_ofs, log, v0, v1, o0, dscratch, &o1);
    call(feed_ofs, log, v1, v2, o1, dscratch, &o2);  //  delta-of-delta

    u8cs lb = {u8bDataHead(log), u8bIdleHead(log)};
    call(expect_resolve, lb, o0, v0);
    call(expect_resolve, lb, o1, v1);
    call(expect_resolve, lb, o2, v2);
    done;
}

//  (2)+(3) Corruption + REF backstop on hand-built packs.
static ok64 bounds() {
    sane(1);
    a_cstr(v0, "base content for corruption probes, long enough to delta");
    a_cstr(v1, "base content for corruption probes, long enough to delt!");

    //  self-ref: an OFS_DELTA whose ofs_delta == 0.
    {
        a_carve(u8, lc, SCRATCH); zerob(lc);
        u8bp log = (u8bp)lc; u8bReset(log);
        u8s into = {u8bIdleHead(log), u8bTerm(log)};
        call(PACKu8sFeedHdr, into, 1); u8bFed(log, 12);
        u64 cur = (u64)u8bDataLen(log);
        a_carve(u8, dc, SCRATCH); u8bp ds = (u8bp)dc; u8bReset(ds);
        call(DELTEncode, v0, v1, ds);
        a_pad(u8, hdr, 16);
        call(PACKu8sFeedObjHdr, hdr, PACK_OBJ_OFS_DELTA, u8bDataLen(ds));
        a_dup(u8c, hb, u8bData(hdr)); call(u8bFeed, log, hb);
        a_pad(u8, ofs, 16); call(PACKu8sFeedOfs, ofs, 0);  //  self-ref
        a_dup(u8c, ob, u8bData(ofs)); call(u8bFeed, log, ob);
        a_dup(u8c, dz, u8bDataC(ds)); call(ZINFDeflate, u8bIdle(log), dz);
        u8cs lb = {u8bDataHead(log), u8bIdleHead(log)};
        call(expect_fail, lb, cur, PACKFAIL);
    }

    //  underflow0: ofs_delta == cur → cur - ofs_delta == 0 (exact
    //  underflow to pack base, the case the loop-top recheck misses).
    {
        a_carve(u8, lc, SCRATCH); zerob(lc);
        u8bp log = (u8bp)lc; u8bReset(log);
        u8s into = {u8bIdleHead(log), u8bTerm(log)};
        call(PACKu8sFeedHdr, into, 1); u8bFed(log, 12);
        u64 cur = (u64)u8bDataLen(log);
        a_carve(u8, dc, SCRATCH); u8bp ds = (u8bp)dc; u8bReset(ds);
        call(DELTEncode, v0, v1, ds);
        a_pad(u8, hdr, 16);
        call(PACKu8sFeedObjHdr, hdr, PACK_OBJ_OFS_DELTA, u8bDataLen(ds));
        a_dup(u8c, hb, u8bData(hdr)); call(u8bFeed, log, hb);
        a_pad(u8, ofs, 16); call(PACKu8sFeedOfs, ofs, cur);  //  → 0
        a_dup(u8c, ob, u8bData(ofs)); call(u8bFeed, log, ob);
        a_dup(u8c, dz, u8bDataC(ds)); call(ZINFDeflate, u8bIdle(log), dz);
        u8cs lb = {u8bDataHead(log), u8bIdleHead(log)};
        call(expect_fail, lb, cur, PACKFAIL);
    }

    //  past-tail: a start offset beyond the pack tail.
    {
        a_carve(u8, lc, SCRATCH); zerob(lc);
        u8bp log = (u8bp)lc; u8bReset(log);
        u8s into = {u8bIdleHead(log), u8bTerm(log)};
        call(PACKu8sFeedHdr, into, 1); u8bFed(log, 12);
        u64 o0 = 0;
        a_carve(u8, dc, SCRATCH); u8bp ds = (u8bp)dc;
        call(feed_raw, log, PACK_OBJ_BLOB, v0, &o0); (void)ds;
        u8cs lb = {u8bDataHead(log), u8bIdleHead(log)};
        call(expect_fail, lb, (u64)u8csLen(lb) + 8, PACKFAIL);
    }

    //  REF backstop: a REF_DELTA record → PACKREF (loud, not silent).
    {
        a_carve(u8, lc, SCRATCH); zerob(lc);
        u8bp log = (u8bp)lc; u8bReset(log);
        u8s into = {u8bIdleHead(log), u8bTerm(log)};
        call(PACKu8sFeedHdr, into, 1); u8bFed(log, 12);
        u64 cur = (u64)u8bDataLen(log);
        //  type=7 (REF_DELTA), size=1, + 20-byte base sha + 1 body byte.
        u8bFeed1(log, (u8)((PACK_OBJ_REF_DELTA << 4) | 1));
        a_pad(u8, sha, 20);
        for (int i = 0; i < 20; i++) u8bFeed1(log, (u8)(0x10 + i));
        u8bFeed1(log, 0x00);
        u8cs lb = {u8bDataHead(log), u8bIdleHead(log)};
        call(expect_fail, lb, cur, PACKREF);
    }
    done;
}

ok64 maintest() {
    sane(1);
    fprintf(stderr, "round_trip...\n");
    call(round_trip);
    fprintf(stderr, "bounds...\n");
    call(bounds);
    fprintf(stderr, "all passed\n");
    done;
}

TEST(maintest)
