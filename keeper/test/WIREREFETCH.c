//  WIREREFETCH — re-fetch have-pruning, now on the GIT-005 thin builder.
//
//  Historically this drove WIREBuildSegments' bookmark-tiling watermark
//  (the GET-007 "re-ship the whole log" bug class).  GIT-005 removed
//  that arm: the incremental fetch goes through WIREBuildThinPack, which
//  ships the want's reachable CLOSURE minus the haves' closure — a
//  byte-range watermark no longer exists, so the GET-007 whole-log
//  re-ship is structurally impossible.  These cases now drive the thin
//  builder and assert the reachability-correct shipped object count
//  (read off the emitted pack header).  The fixtures are isolated blobs
//  (no commit graph), so a blob want resolves to itself and a blob have
//  prunes only itself — exactly the want/have set algebra under test.

#include "keeper/WIRE.h"
#include "keeper/KEEP.h"
#include "dog/git/PACK.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PRO.h"
#include "abc/TEST.h"
#include "keeper/REFADV.h"
#include "dog/git/SHA1.h"

static void tmp_rm(char const *path) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    int _ = system(cmd);
    (void)_;
}

//  Add a pack with a single blob; report its sha.  KEEPPackOpen reuses
//  the tail file when packs exist, so repeated calls append packs into
//  trunk file_id 1 — exactly keeper's append-of-packs log shape.
static ok64 add_blob_pack(sha1 *out_sha, char const *content) {
    sane(out_sha);
    keep_pack p = {};
    call(KEEPPackOpen, &p);
    a_cstr(blob_s, content);
    sha1 sha = {};
    call(KEEPPackFeed, &p, KEEP_OBJ_BLOB, blob_s, 0, &sha);
    *out_sha = sha;
    call(KEEPPackClose, &p);
    done;
}

//  Build the thin pack for (want, have) and return its header object
//  count.  Drives the GIT-005 incremental serve arm directly.
static ok64 thin_count(refadvcp adv, sha1cp want, sha1cp have,
                       u32 *out_count) {
    sane(adv && want && out_count);
    wire_req req = {};
    req.nwants = 1; req.wants[0] = *want;
    req.caps   = WIRE_CAP_OFS_DELTA;
    if (have) { req.nhaves = 1; req.haves[0] = *have; }

    Bu8 packbuf = {};
    call(u8bAllocate, packbuf, 1ULL << 20);
    try(WIREBuildThinPack, adv, &req, packbuf);
    ok64 rv = __;
    if (rv != OK) { u8bFree(packbuf); return rv; }
    a_dup(u8c, scan, u8bData(packbuf));
    pack_hdr hh = {};
    ok64 po = PACKDrainHdr(scan, &hh);
    *out_count = hh.count;
    u8bFree(packbuf);
    return po;
}

//  Re-fetch of an unchanged repo: the client already holds the tip, so
//  it advertises the tip as a have AND wants the tip.  The server must
//  ship ~0 objects.
ok64 WIREREFETCHtest_unchanged() {
    sane(1);
    call(FILEInit);

    char tmpdir[] = "/tmp/wire-refetch-XXXXXX";
    want(mkdtemp(tmpdir) != NULL);
    a_cstr(root, tmpdir);
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);

    //  Build a multi-pack trunk: 5 packs in file_id 1.
    sha1 shas[5] = {};
    char buf[64];
    for (u32 i = 0; i < 5; i++) {
        snprintf(buf, sizeof(buf), "object number %u\n", i);
        call(add_blob_pack, &shas[i], buf);
    }
    sha1 tip = shas[4];

    a_refadv(adv);
    call(REFADVOpen, &adv);

    //  Re-fetch: want == tip, have == tip (client already has it).
    u32 count = 99;
    call(thin_count, &adv, &tip, &tip, &count);
    fprintf(stderr, "[REFETCH] unchanged re-fetch ships count=%u\n", count);
    //  An unchanged re-fetch ships 0 objects — the tip is in the have
    //  closure, so nothing remains in the ship-set.
    want(count == 0);

    REFADVClose(&adv);
    call(KEEPClose);
    HOMEClose();
    tmp_rm(tmpdir);
    done;
}

//  Incremental fetch: client has the first 3 packs' tip, server has 5.
//  Must ship only the 2 new objects, not the whole log.
ok64 WIREREFETCHtest_incremental() {
    sane(1);
    call(FILEInit);

    char tmpdir[] = "/tmp/wire-incr-XXXXXX";
    want(mkdtemp(tmpdir) != NULL);
    a_cstr(root, tmpdir);
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);

    sha1 shas[5] = {};
    char buf[64];
    for (u32 i = 0; i < 5; i++) {
        snprintf(buf, sizeof(buf), "incr object %u\n", i);
        call(add_blob_pack, &shas[i], buf);
    }

    a_refadv(adv);
    call(REFADVOpen, &adv);

    //  Client has shas[2]; wants shas[4].  These blobs are mutually
    //  unreachable (no commit graph), so the reachability ship-set of
    //  want=shas[4] minus have-closure {shas[2]} is just {shas[4]}.  The
    //  old bookmark-tiling counted 2 (every object byte-after the have's
    //  watermark); reachability ships exactly the 1 wanted object.
    u32 count = 99;
    call(thin_count, &adv, &shas[4], &shas[2], &count);
    fprintf(stderr, "[REFETCH] incremental fetch ships count=%u\n", count);
    want(count == 1);

    REFADVClose(&adv);
    call(KEEPClose);
    HOMEClose();
    tmp_rm(tmpdir);
    done;
}

//  Duplicate re-ingest: the same objects get re-appended in a later
//  pack (the bloat cycle).  wire_locate_sha resolves want/have to the
//  EARLIEST offset of the duplicated sha, so the watermark anchors too
//  early and the server re-ships everything appended after — including
//  the later duplicate copies.  This is the offset != reachability trap.
ok64 WIREREFETCHtest_dup_reingest() {
    sane(1);
    call(FILEInit);

    char tmpdir[] = "/tmp/wire-dup-XXXXXX";
    want(mkdtemp(tmpdir) != NULL);
    a_cstr(root, tmpdir);
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);

    //  Pack the same 3 objects TWICE (re-fetch re-appended them).
    sha1 shas[3] = {};
    char buf[64];
    for (u32 round = 0; round < 2; round++) {
        for (u32 i = 0; i < 3; i++) {
            snprintf(buf, sizeof(buf), "dup object %u\n", i);
            call(add_blob_pack, &shas[i], buf);
        }
    }
    sha1 tip = shas[2];

    a_refadv(adv);
    call(REFADVOpen, &adv);

    //  Re-fetch unchanged: want == have == tip.  Nothing new exists; the
    //  client has every object.  Reachability ships 0 — and since the
    //  ship-set is closure-based, the duplicate later copies are
    //  irrelevant (the GET-007 byte-range re-ship cannot occur).
    u32 count = 99;
    call(thin_count, &adv, &tip, &tip, &count);
    fprintf(stderr, "[REFETCH] dup-reingest re-fetch ships count=%u\n", count);
    want(count == 0);

    REFADVClose(&adv);
    call(KEEPClose);
    HOMEClose();
    tmp_rm(tmpdir);
    done;
}

//  Incremental fetch over a DUPLICATE-laden history (the GET-007 bloat
//  accumulator).  The store holds objects re-ingested twice (offsets
//  interleaved), then one genuinely-new object on top.  The client's
//  have is the pre-new tip.  The server must ship ONLY the new object,
//  not the duplicate packs sitting between the have's early copy and the
//  log tail.  Before the latest-copy fix the want/have resolve to their
//  EARLIEST offsets, so the watermark anchors too low and the whole
//  duplicate-laden tail is re-shipped.
ok64 WIREREFETCHtest_dup_then_incremental() {
    sane(1);
    call(FILEInit);

    char tmpdir[] = "/tmp/wire-dupincr-XXXXXX";
    want(mkdtemp(tmpdir) != NULL);
    a_cstr(root, tmpdir);
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);

    //  Round 0 + round 1: the same 3 objects, re-ingested (duplicates).
    sha1 shas[3] = {};
    char buf[64];
    for (u32 round = 0; round < 2; round++) {
        for (u32 i = 0; i < 3; i++) {
            snprintf(buf, sizeof(buf), "dupincr object %u\n", i);
            call(add_blob_pack, &shas[i], buf);
        }
    }
    //  The client's tip BEFORE the new commit is shas[2] (present twice).
    sha1 old_tip = shas[2];

    //  One genuinely-new object appended on top (the new commit).
    sha1 new_tip = {};
    call(add_blob_pack, &new_tip, "dupincr NEW object\n");

    a_refadv(adv);
    call(REFADVOpen, &adv);

    //  Want the genuinely-new object; have the old tip.  Reachability
    //  ships exactly the 1 new object — the duplicate-laden tail between
    //  the have's earliest copy and the log end is never re-shipped
    //  (closure, not byte range).
    u32 count = 99;
    call(thin_count, &adv, &new_tip, &old_tip, &count);
    fprintf(stderr, "[REFETCH] dup-then-incremental ships count=%u\n", count);
    want(count == 1);

    REFADVClose(&adv);
    call(KEEPClose);
    HOMEClose();
    tmp_rm(tmpdir);
    done;
}

//  Cross-file_id (cause a): the want sits in trunk file_id 1, the have
//  sits in a fresh leaf branch whose first pack got a NEW file_id 2.
//  WIRE.c's `hfid != want_fid` test drops the cross-file have, so
//  have_anchor stays NO and the server re-ships the WHOLE trunk log even
//  though the client already holds an overlapping have.  This is the
//  multi-shard whole-log re-ship the ticket flags.
ok64 WIREREFETCHtest_cross_file() {
    sane(1);
    call(FILEInit);

    char tmpdir[] = "/tmp/wire-xfile-XXXXXX";
    want(mkdtemp(tmpdir) != NULL);
    a_cstr(root, tmpdir);
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);

    //  Trunk file_id 1: two packs (A, B).  B is the want (trunk tip).
    sha1 a = {}, b = {};
    call(add_blob_pack, &a, "xfile trunk A\n");
    call(add_blob_pack, &b, "xfile trunk B\n");

    //  Create + switch to a leaf branch; its first pack mints file_id 2.
    //  The leaf object C is what the client advertises as a have (it has
    //  the leaf branch already).
    a_cstr(leaf_s, "feat");
    {
        u8cs leaf = {(u8c *)leaf_s[0], (u8c *)leaf_s[1]};
        ok64 cb = KEEPCreateBranch(leaf);
        want(cb == OK || cb == KEEPDUP);
        u8cs leaf2 = {(u8c *)leaf_s[0], (u8c *)leaf_s[1]};
        call(KEEPSwitchBranch, leaf2);
    }
    sha1 c = {};
    call(add_blob_pack, &c, "xfile leaf C\n");

    a_refadv(adv);
    call(REFADVOpen, &adv);

    //  Want trunk tip B; advertise leaf C (a different file_id) as a
    //  have.  The thin builder walks closures by SHA, not by file_id, so
    //  the cross-file have is handled uniformly: C is unreachable from B,
    //  so it prunes nothing and the ship-set is {B} (count 1).  The old
    //  `hfid != want_fid` whole-log re-ship is gone.
    u32 count = 99;
    call(thin_count, &adv, &b, &c, &count);
    fprintf(stderr, "[REFETCH] cross-file ships count=%u\n", count);
    want(count == 1);

    REFADVClose(&adv);
    call(KEEPClose);
    HOMEClose();
    tmp_rm(tmpdir);
    done;
}

ok64 maintest() {
    sane(1);
    fprintf(stderr, "WIREREFETCHtest_unchanged...\n");
    call(WIREREFETCHtest_unchanged);
    fprintf(stderr, "WIREREFETCHtest_incremental...\n");
    call(WIREREFETCHtest_incremental);
    fprintf(stderr, "WIREREFETCHtest_dup_reingest...\n");
    call(WIREREFETCHtest_dup_reingest);
    fprintf(stderr, "WIREREFETCHtest_dup_then_incremental...\n");
    call(WIREREFETCHtest_dup_then_incremental);
    fprintf(stderr, "WIREREFETCHtest_cross_file...\n");
    call(WIREREFETCHtest_cross_file);
    fprintf(stderr, "all passed\n");
    done;
}

TEST(maintest)
