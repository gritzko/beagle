//  DELTA_ROUND: end-to-end delta round-trip through real git.
//
//  GIT-002 (OFS-ALWAYS): the writer emits raw objects + OFS_DELTA
//  EXCLUSIVELY — never REF_DELTA.  This test feeds N versions of a
//  blob across what were pack-batch boundaries into the ONE growing
//  shard log, base-hinting each version at its predecessor, then:
//
//  1. Open a fresh keeper, open the (single, growing) shard log.
//  2. Feed N versions of a blob, each hinted at the previous version
//     so the writer tries OFS_DELTA against an earlier object in the
//     same log.  The hops cross what used to be separate packs.
//  3. Close the log; on disk it is a git-compatible pack minus the
//     trailing SHA-1.
//  4. Append SHA1(logbytes) as the 20-byte trailer and hand the
//     resulting .pack to `git index-pack`.  Put pack+idx into a bare
//     git repo.
//  5. Ask git to print every version via `git cat-file blob <sha>`
//     and verify the bytes match what we fed (round-trip).
//  6. Assert the OFS-only spec: every delta record is OFS_DELTA and
//     ZERO REF_DELTA records exist in the on-disk log, and that
//     cross-batch deltas actually formed (more than one OFS_DELTA).

#include "keeper/KEEP.h"
#include "dog/git/PACK.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "abc/B.h"
#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/S.h"
#include "abc/TEST.h"
#include "dog/HOME.h"
#include "dog/git/SHA1.h"

static void sha_to_hex(char *hex41, sha1cp s) {
    static const char HEX[] = "0123456789abcdef";
    for (int i = 0; i < 20; i++) {
        hex41[2*i]   = HEX[s->data[i] >> 4];
        hex41[2*i+1] = HEX[s->data[i] & 0xf];
    }
    hex41[40] = 0;
}

//  Walk the on-disk log and count object types.  After the 12-byte
//  PACK header, records are {type|size varint, [ofs/ref], zlib body}.
//  We only need to count — advance past each zlib stream by inflating
//  its declared size.
static ok64 count_types(u8csc log, u32 counts[8]) {
    sane($ok(log));
    memset(counts, 0, sizeof(u32) * 8);

    a_dup(u8c, scan, log);
    pack_hdr hdr = {};
    call(PACKDrainHdr, scan, &hdr);

    Bu8 tmp = {};
    call(u8bMap, tmp, 1 << 20);

    for (u32 i = 0; i < hdr.count; i++) {
        pack_obj obj = {};
        if (PACKDrainObjHdr(scan, &obj) != OK) break;
        if (obj.type < 8) counts[obj.type]++;
        //  Advance `scan` past the zlib body by inflating `obj.size`
        //  bytes (we don't care about the content).
        u8bReset(tmp);
        u8s into = {u8bIdleHead(tmp), u8bTerm(tmp)};
        if (PACKInflate(scan, into, obj.size) != OK) break;
    }

    u8bUnMap(tmp);
    done;
}

ok64 DELTARoundTrip() {
    sane(1);
    call(FILEInit);

    char tmp[] = "/tmp/keeper-delta-round-XXXXXX";
    want(mkdtemp(tmp) != NULL);

    u8cs root = {(u8cp)tmp, (u8cp)tmp + strlen(tmp)};
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);

    enum { N = 4 };
    char const *versions[N] = {
        "the quick brown fox jumps over the lazy dog, "
        "once upon a time in a land far away, "
        "there lived a small fox who dreamed of adventure.",

        "the quick brown fox jumps over the lazy dog, "
        "once upon a time in a land far away, "
        "there lived a small fox who dreamed of BIGGER adventures.",

        "the quick RED fox jumps over the lazy dog, "
        "once upon a time in a land far away, "
        "there lived a small fox who dreamed of BIGGER adventures.",

        "the quick RED fox jumps over the SLEEPY dog, "
        "once upon a time in a land far away, "
        "there lived a small fox who dreamed of BIGGER adventures "
        "and long sunny days.",
    };

    sha1 shas[N] = {};

    //  ONE growing shard log (GIT-002): feed every version into a
    //  single open session, each hinted at its predecessor.  This is
    //  the cross-batch case — under the old pack-batch-scoped writer
    //  the later versions could only REF_DELTA a committed base; now
    //  the base is an earlier object in the same open log addressed by
    //  offset, so the writer emits OFS_DELTA against any RAW base.
    //
    //  Expected layout (base hint = previous version's sha):
    //    v0  raw blob       (first object, no base)
    //    v1  OFS_DELTA      (base v0 — raw, earlier in the log)
    //    v2  raw blob       (base v1 is itself a delta; raw bases only)
    //    v3  OFS_DELTA      (base v2 — raw, earlier in the log)
    //  → 2 raw blobs, 2 OFS_DELTA, ZERO REF_DELTA.
    {
        keep_pack p = {};
        call(KEEPPackOpen, &p);
        p.strict_order = NO;
        for (int i = 0; i < N; i++) {
            u8csc c = {(u8cp)versions[i],
                       (u8cp)versions[i] + strlen(versions[i])};
            u64 hint = i == 0 ? 0 : WHIFFHashlet60(&shas[i-1]);
            call(KEEPPackFeed, &p, DOG_OBJ_BLOB, c, hint, &shas[i]);
        }
        call(KEEPPackClose, &p);
    }

    //  Read the on-disk log.
    char logpath[1024];
    snprintf(logpath, sizeof(logpath),
             "%s/" DOG_BE_NAME "/0000000001.keeper", tmp);
    u8bp logmap = NULL;
    {
        u8cs lpp = {(u8cp)logpath, (u8cp)logpath + strlen(logpath)};
        a_pad(u8, lpbuf, 1100);
        u8bFeed(lpbuf, lpp);
        u8bFeed1(lpbuf, 0);
        u8cs lpt = {u8bDataHead(lpbuf), u8bDataHead(lpbuf) + u8bDataLen(lpbuf) - 1};
        call(FILEMapRO, &logmap, lpt);
    }

    //  OFS-only spec assertions (GIT-002):
    //    (a) ZERO REF_DELTA records in the log,
    //    (b) cross-batch deltas formed (>1 OFS_DELTA, hinting at an
    //        earlier object in the same growing log),
    //    (c) the exact expected layout: 2 raw blobs + 2 OFS_DELTA.
    u32 counts[8] = {};
    a_dup(u8c, logbytes, u8bData(logmap));
    call(count_types, logbytes, counts);
    fprintf(stderr,
            "delta_round: log=%llu bytes, objs: blob=%u ofs=%u ref=%u\n",
            (unsigned long long)u8csLen(logbytes),
            counts[PACK_OBJ_BLOB],
            counts[PACK_OBJ_OFS_DELTA],
            counts[PACK_OBJ_REF_DELTA]);
    //  (a) The native writer must NEVER emit REF_DELTA.
    if (counts[PACK_OBJ_REF_DELTA] != 0) {
        fprintf(stderr,
                "delta_round: OFS-only violated — %u REF_DELTA in log\n",
                counts[PACK_OBJ_REF_DELTA]);
        fail(TESTFAIL);
    }
    //  (b) Cross-batch OFS deltas actually formed.
    if (counts[PACK_OBJ_OFS_DELTA] < 2) {
        fprintf(stderr,
                "delta_round: expected >=2 OFS_DELTA, got %u\n",
                counts[PACK_OBJ_OFS_DELTA]);
        fail(TESTFAIL);
    }
    //  (c) Exact expected layout: v0,v2 raw; v1,v3 OFS_DELTA.
    if (counts[PACK_OBJ_BLOB] != 2) {
        fprintf(stderr, "delta_round: expected 2 raw blobs, got %u\n",
                counts[PACK_OBJ_BLOB]);
        fail(TESTFAIL);
    }
    if (counts[PACK_OBJ_OFS_DELTA] != 2) {
        fprintf(stderr, "delta_round: expected 2 OFS_DELTA, got %u\n",
                counts[PACK_OBJ_OFS_DELTA]);
        fail(TESTFAIL);
    }

    //  Build a valid git pack: logbytes + SHA1(logbytes) trailer.
    Bu8 gitpack = {};
    call(u8bMap, gitpack, 1 << 20);
    u8bFeed(gitpack, logbytes);
    sha1 pack_sha = {};
    a_dup(u8c, gc, u8bDataC(gitpack));
    SHA1Sum(&pack_sha, gc);
    u8cs trailer = {pack_sha.data, pack_sha.data + 20};
    u8bFeed(gitpack, trailer);

    //  Write it into a bare repo under $tmp/bare.git/objects/pack/.
    //  `cd '%s'` so git doesn't discover this test binary's CWD (which
    //  on a worktree checkout may point at a non-existent gitdir).
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "cd '%s' && git init --bare bare.git >/dev/null 2>&1", tmp);
    want(system(cmd) == 0);

    char packout[1024];
    snprintf(packout, sizeof(packout),
             "%s/bare.git/objects/pack/pack.pack", tmp);
    int pfd = -1;
    {
        u8cs pp = {(u8cp)packout, (u8cp)packout + strlen(packout)};
        a_pad(u8, ppb, 1100);
        u8bFeed(ppb, pp);
        u8bFeed1(ppb, 0);
        u8cs ppt = {u8bDataHead(ppb), u8bDataHead(ppb) + u8bDataLen(ppb) - 1};
        call(FILECreate, &pfd, ppt);
    }
    a_dup(u8c, pd, u8bData(gitpack));
    call(FILEFeedAll, pfd, pd);
    close(pfd);

    //  Let git build the idx.  Enter objects/pack/ so git discovers
    //  bare.git walking up (avoids any inherited GIT_DIR ambiguity).
    snprintf(cmd, sizeof(cmd),
             "cd '%s/bare.git/objects/pack' && "
             "git index-pack pack.pack >/dev/null 2>&1",
             tmp);
    if (system(cmd) != 0) {
        fprintf(stderr, "delta_round: git index-pack failed\n");
        u8bUnMap(gitpack);
        FILEUnMap(logmap);
        fail(TESTFAIL);
    }

    //  For each version, fetch via `git cat-file blob <sha>` and
    //  compare to the original bytes.
    for (int i = 0; i < N; i++) {
        char hex[41];
        sha_to_hex(hex, &shas[i]);
        char outpath[1024];
        snprintf(outpath, sizeof(outpath), "%s/got_%d.bin", tmp, i);
        snprintf(cmd, sizeof(cmd),
                 "cd '%s/bare.git' && git cat-file blob %s > '%s' 2>/dev/null",
                 tmp, hex, outpath);
        if (system(cmd) != 0) {
            fprintf(stderr,
                    "delta_round: git cat-file failed for v%d %s\n", i, hex);
            u8bUnMap(gitpack);
            FILEUnMap(logmap);
            fail(TESTFAIL);
        }

        //  Compare byte-for-byte.
        int fd = -1;
        u8cs opp = {(u8cp)outpath, (u8cp)outpath + strlen(outpath)};
        a_pad(u8, opb, 1100);
        u8bFeed(opb, opp);
        u8bFeed1(opb, 0);
        u8cs opt = {u8bDataHead(opb), u8bDataHead(opb) + u8bDataLen(opb) - 1};
        call(FILEOpen, &fd, opt, O_RDONLY);

        Bu8 gotbuf = {};
        call(u8bMap, gotbuf, 1 << 16);
        call(FILEdrainall, u8bIdle(gotbuf), fd);
        close(fd);

        u64 want_len = strlen(versions[i]);
        if (u8bDataLen(gotbuf) != want_len ||
            memcmp(u8bDataHead(gotbuf), versions[i], want_len) != 0) {
            fprintf(stderr,
                    "delta_round: v%d mismatch — git returned %llu bytes, "
                    "expected %llu\n",
                    i,
                    (unsigned long long)u8bDataLen(gotbuf),
                    (unsigned long long)want_len);
            u8bUnMap(gotbuf);
            u8bUnMap(gitpack);
            FILEUnMap(logmap);
            fail(TESTFAIL);
        }
        u8bUnMap(gotbuf);
    }

    u8bUnMap(gitpack);
    FILEUnMap(logmap);
    call(KEEPClose);
    HOMEClose();

    //  rm -rf the tmpdir.
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", tmp);
    system(cmd);

    done;
}

ok64 maintest() {
    sane(1);
    call(DELTARoundTrip);
    done;
}

TEST(maintest)
