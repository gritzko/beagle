//  WIREREFETCH — repro/trace for GET-007: server-side have-pruning that
//  re-ships the whole log on a re-fetch of an unchanged repo.
//
//  Builds a multi-pack trunk store (each "commit" ingested as its own
//  pack into the same trunk file_id, mirroring incremental be post/get),
//  then drives WIREBuildSegments for the re-fetch case where the client
//  already has the tip (want == latest have).  Asserts the shipped
//  segment is ~empty (count == 0).  Before the fix this ships the whole
//  log (count == N).  Set WIRE_TRACE=1 to dump the watermark trace.

#include "keeper/WIRE.h"
#include "keeper/KEEP.h"

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
    call(HOMEOpenAt, &h, root, YES);
    call(KEEPOpen, &h, YES);

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
    wire_req req = {};
    req.nwants  = 1;
    req.wants[0] = tip;
    req.nhaves  = 1;
    req.haves[0] = tip;

    pstr_seg segs[4] = {};
    int      fds [4] = {-1,-1,-1,-1};
    u32      n = 0;
    call(WIREBuildSegments, &adv, &req, segs, fds, 4, &n);
    want(n == 1);

    fprintf(stderr, "[REFETCH] unchanged re-fetch ships count=%u bytes=%llu\n",
            segs[0].count, (unsigned long long)segs[0].length);

    //  THE BUG: an unchanged re-fetch must ship ~0 objects.  The tip is
    //  already present on the client; nothing new exists.
    want(segs[0].count == 0);

    for (int i = 0; i < 4; i++) if (fds[i] >= 0) close(fds[i]);
    REFADVClose(&adv);
    call(KEEPClose);
    HOMEClose(&h);
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
    call(HOMEOpenAt, &h, root, YES);
    call(KEEPOpen, &h, YES);

    sha1 shas[5] = {};
    char buf[64];
    for (u32 i = 0; i < 5; i++) {
        snprintf(buf, sizeof(buf), "incr object %u\n", i);
        call(add_blob_pack, &shas[i], buf);
    }

    a_refadv(adv);
    call(REFADVOpen, &adv);

    //  Client has up to pack index 2 (shas[2]); wants shas[4].
    wire_req req = {};
    req.nwants  = 1;
    req.wants[0] = shas[4];
    req.nhaves  = 1;
    req.haves[0] = shas[2];

    pstr_seg segs[4] = {};
    int      fds [4] = {-1,-1,-1,-1};
    u32      n = 0;
    call(WIREBuildSegments, &adv, &req, segs, fds, 4, &n);
    want(n == 1);

    fprintf(stderr, "[REFETCH] incremental fetch ships count=%u bytes=%llu\n",
            segs[0].count, (unsigned long long)segs[0].length);

    //  Only packs 3 and 4 (2 objects) are new.
    want(segs[0].count == 2);

    for (int i = 0; i < 4; i++) if (fds[i] >= 0) close(fds[i]);
    REFADVClose(&adv);
    call(KEEPClose);
    HOMEClose(&h);
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
    call(HOMEOpenAt, &h, root, YES);
    call(KEEPOpen, &h, YES);

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
    //  client has every object.  The server MUST ship ~0.
    wire_req req = {};
    req.nwants  = 1;
    req.wants[0] = tip;
    req.nhaves  = 1;
    req.haves[0] = tip;

    pstr_seg segs[4] = {};
    int      fds [4] = {-1,-1,-1,-1};
    u32      n = 0;
    call(WIREBuildSegments, &adv, &req, segs, fds, 4, &n);
    want(n == 1);

    fprintf(stderr, "[REFETCH] dup-reingest re-fetch ships count=%u bytes=%llu\n",
            segs[0].count, (unsigned long long)segs[0].length);

    //  THE BUG: the duplicate copies in the later packs get re-shipped.
    want(segs[0].count == 0);

    for (int i = 0; i < 4; i++) if (fds[i] >= 0) close(fds[i]);
    REFADVClose(&adv);
    call(KEEPClose);
    HOMEClose(&h);
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
    call(HOMEOpenAt, &h, root, YES);
    call(KEEPOpen, &h, YES);

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

    wire_req req = {};
    req.nwants  = 1;
    req.wants[0] = new_tip;
    req.nhaves  = 1;
    req.haves[0] = old_tip;

    pstr_seg segs[4] = {};
    int      fds [4] = {-1,-1,-1,-1};
    u32      n = 0;
    call(WIREBuildSegments, &adv, &req, segs, fds, 4, &n);
    want(n == 1);

    fprintf(stderr, "[REFETCH] dup-then-incremental ships count=%u bytes=%llu\n",
            segs[0].count, (unsigned long long)segs[0].length);

    //  THE FIX: only the 1 new object ships, not the duplicate tail.
    want(segs[0].count == 1);

    for (int i = 0; i < 4; i++) if (fds[i] >= 0) close(fds[i]);
    REFADVClose(&adv);
    call(KEEPClose);
    HOMEClose(&h);
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
    call(HOMEOpenAt, &h, root, YES);
    call(KEEPOpen, &h, YES);

    //  Trunk file_id 1: two packs (A, B).  B is the want (trunk tip).
    sha1 a = {}, b = {};
    call(add_blob_pack, &a, "xfile trunk A\n");
    call(add_blob_pack, &b, "xfile trunk B\n");

    //  Create + switch to a leaf branch; its first pack mints file_id 2.
    //  The leaf object C is what the client advertises as a have (it has
    //  the leaf branch already).
    a_cstr(leaf_s, "feat");
    {
        u8s leaf = {(u8 *)leaf_s[0], (u8 *)leaf_s[1]};
        ok64 cb = KEEPCreateBranch(&h, leaf);
        want(cb == OK || cb == KEEPDUP);
        u8s leaf2 = {(u8 *)leaf_s[0], (u8 *)leaf_s[1]};
        call(KEEPSwitchBranch, &h, leaf2);
    }
    sha1 c = {};
    call(add_blob_pack, &c, "xfile leaf C\n");

    a_refadv(adv);
    call(REFADVOpen, &adv);

    //  Want trunk tip B; advertise leaf C as a have.  (A real re-fetch
    //  also advertises B, but to isolate cause a we send only the
    //  cross-file have.)  The server must NOT default to the whole log.
    wire_req req = {};
    req.nwants  = 1;
    req.wants[0] = b;
    req.nhaves  = 1;
    req.haves[0] = c;

    pstr_seg segs[4] = {};
    int      fds [4] = {-1,-1,-1,-1};
    u32      n = 0;
    ok64 bo = WIREBuildSegments(&adv, &req, segs, fds, 4, &n);

    fprintf(stderr, "[REFETCH] cross-file rc=%s n=%u count=%u\n",
            bo == OK ? "OK" : "ERR", n,
            n > 0 ? segs[0].count : 0);

    //  Document the current cross-file behaviour: the leaf have is in a
    //  different file_id, so it is dropped and the trunk want resolves
    //  normally.  This case exercises the `hfid != want_fid` branch;
    //  a full multi-file watermark fix would let a cross-file have that
    //  is actually an ancestor anchor the segment.  Kept as a trace
    //  probe — no hard assert beyond a clean return.
    want(bo == OK);

    for (int i = 0; i < 4; i++) if (fds[i] >= 0) close(fds[i]);
    REFADVClose(&adv);
    call(KEEPClose);
    HOMEClose(&h);
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
