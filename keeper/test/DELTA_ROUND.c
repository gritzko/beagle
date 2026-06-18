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

//  Walk the log recording each object's start offset and record type,
//  then for every OFS_DELTA follow its back-pointer (`cur - ofs_delta`)
//  to its base record and report: how many OFS_DELTA records have a
//  base that is ITSELF an OFS_DELTA (true delta-of-delta chaining), and
//  the deepest OFS chain length reached.  Pure structural inspection —
//  no inflate of bodies beyond skipping them.
static ok64 chain_stats(u8csc log, u32 *deltas_on_deltas, u32 *max_depth) {
    sane($ok(log));
    *deltas_on_deltas = 0;
    *max_depth = 0;

    a_dup(u8c, scan, log);
    pack_hdr hdr = {};
    call(PACKDrainHdr, scan, &hdr);
    u8cp pack0 = log[0];

    enum { MAXOBJ = 4096 };
    static u64 offs[MAXOBJ];
    static u8  types[MAXOBJ];   // record type at that offset
    static u64 bases[MAXOBJ];   // for OFS_DELTA: base offset, else U64MAX
    u32 n = 0;

    Bu8 tmp = {};
    call(u8bMap, tmp, 1 << 20);

    for (u32 i = 0; i < hdr.count && n < MAXOBJ; i++) {
        u64 here = (u64)(scan[0] - pack0);
        pack_obj obj = {};
        if (PACKDrainObjHdr(scan, &obj) != OK) break;
        offs[n]  = here;
        types[n] = obj.type;
        bases[n] = (obj.type == PACK_OBJ_OFS_DELTA)
                       ? (here - obj.ofs_delta)
                       : (u64)-1;
        n++;
        u8bReset(tmp);
        u8s into = {u8bIdleHead(tmp), u8bTerm(tmp)};
        if (PACKInflate(scan, into, obj.size) != OK) break;
    }
    u8bUnMap(tmp);

    //  type-at-offset lookup + chain length per object.
    for (u32 i = 0; i < n; i++) {
        if (types[i] != PACK_OBJ_OFS_DELTA) continue;
        //  Find the base record by offset.
        u64 boff = bases[i];
        for (u32 j = 0; j < n; j++) {
            if (offs[j] != boff) continue;
            if (types[j] == PACK_OBJ_OFS_DELTA) (*deltas_on_deltas)++;
            break;
        }
        //  Chain length: walk back through OFS bases.
        u32 d = 0;
        u64 cur = offs[i];
        for (;;) {
            u32 k = (u32)-1;
            for (u32 j = 0; j < n; j++) {
                if (offs[j] == cur) { k = j; break; }
            }
            if (k == (u32)-1 || types[k] != PACK_OBJ_OFS_DELTA) break;
            d++;
            cur = bases[k];
            if (d > MAXOBJ) break;  // cycle guard
        }
        if (d > *max_depth) *max_depth = d;
    }
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

    //  DEEP revision chain (GIT-008): N versions of ONE file, each a
    //  small edit of the previous, fed in order with the predecessor as
    //  the base hint.  Under the OLD writer a delta base had to be RAW,
    //  so chains never formed — the layout degenerated to
    //  raw,delta,raw,delta,… (every other version stored as a full
    //  blob).  Under GIT-008 the writer deltas against a base that is
    //  itself a delta, so a single chain forms:
    //    v0  raw blob
    //    v1  OFS_DELTA → v0
    //    v2  OFS_DELTA → v1   (v1 is itself a delta — the new case)
    //    …
    //  → ONE raw blob, N-1 OFS_DELTA, ZERO REF_DELTA, true chaining.
    enum { N = 12 };
    static char verbuf[N][512];
    char const *versions[N] = {};
    {
        //  A shared paragraph plus a small per-version mutation: change
        //  one word and append a short version-specific tail.  Each
        //  version differs from its predecessor by a few bytes only, so
        //  the delta is tiny and the chain is the cheap encoding.
        static char const *const COLORS[N] = {
            "brown", "red", "amber", "golden", "silver", "violet",
            "crimson", "azure", "emerald", "scarlet", "indigo", "ivory",
        };
        for (int i = 0; i < N; i++) {
            snprintf(verbuf[i], sizeof(verbuf[i]),
                     "the quick %s fox jumps over the lazy dog, "
                     "once upon a time in a land far away, "
                     "there lived a small fox who dreamed of "
                     "adventure number %d and many sunny days to come.",
                     COLORS[i], i);
            versions[i] = verbuf[i];
        }
    }

    sha1 shas[N] = {};

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

    //  Reference all-raw size, measured in a SEPARATE store (a second
    //  KEEPPackOpen on THIS keeper would append to file 1 and corrupt
    //  the chained log).  Feed the SAME N versions with NO base hint so
    //  every object stores raw; the chained log above must be smaller.
    u64 allraw_len = 0;
    {
        call(KEEPClose);
        HOMEClose();
        char raw[] = "/tmp/keeper-delta-raw-XXXXXX";
        want(mkdtemp(raw) != NULL);
        u8cs rroot = {(u8cp)raw, (u8cp)raw + strlen(raw)};
        call(HOMEOpenAt, rroot, YES);
        call(KEEPOpen, YES);
        keep_pack p = {};
        call(KEEPPackOpen, &p);
        p.strict_order = NO;
        sha1 rs = {};
        for (int i = 0; i < N; i++) {
            u8csc c = {(u8cp)versions[i],
                       (u8cp)versions[i] + strlen(versions[i])};
            call(KEEPPackFeed, &p, DOG_OBJ_BLOB, c, 0, &rs);
        }
        allraw_len = u8bDataLen(p.log);
        call(KEEPPackClose, &p);
        call(KEEPClose);
        HOMEClose();
        char rmcmd[1100];
        snprintf(rmcmd, sizeof(rmcmd), "rm -rf '%s'", raw);
        system(rmcmd);
        //  Reopen the original store for the round-trip + KEEPGet checks.
        call(HOMEOpenAt, root, YES);
        call(KEEPOpen, YES);
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

    //  OFS-only + chaining assertions (GIT-008):
    //    (a) ZERO REF_DELTA records in the log,
    //    (b) one raw root + N-1 OFS_DELTA (a single chain, not the old
    //        raw,delta,raw,delta degenerate layout),
    //    (c) real delta-of-delta chaining: at least one OFS_DELTA whose
    //        base is itself an OFS_DELTA, and a chain that runs DEEP
    //        (>= 3 hops), proving the writer extends past a delta base,
    //    (d) the chained log is materially smaller than the all-raw log.
    u32 counts[8] = {};
    a_dup(u8c, logbytes, u8bData(logmap));
    call(count_types, logbytes, counts);
    u32 deltas_on_deltas = 0, max_depth = 0;
    {
        a_dup(u8c, lb2, u8bData(logmap));
        call(chain_stats, lb2, &deltas_on_deltas, &max_depth);
    }
    fprintf(stderr,
            "delta_round: log=%llu bytes (all-raw=%llu), "
            "blob=%u ofs=%u ref=%u, delta-on-delta=%u max-depth=%u\n",
            (unsigned long long)u8csLen(logbytes),
            (unsigned long long)allraw_len,
            counts[PACK_OBJ_BLOB],
            counts[PACK_OBJ_OFS_DELTA],
            counts[PACK_OBJ_REF_DELTA],
            deltas_on_deltas, max_depth);
    //  (a) The native writer must NEVER emit REF_DELTA.
    if (counts[PACK_OBJ_REF_DELTA] != 0) {
        fprintf(stderr,
                "delta_round: OFS-only violated — %u REF_DELTA in log\n",
                counts[PACK_OBJ_REF_DELTA]);
        fail(TESTFAIL);
    }
    //  (b) Single chain: exactly one raw root, the rest OFS_DELTA.
    if (counts[PACK_OBJ_BLOB] != 1) {
        fprintf(stderr, "delta_round: expected 1 raw blob, got %u\n",
                counts[PACK_OBJ_BLOB]);
        fail(TESTFAIL);
    }
    if (counts[PACK_OBJ_OFS_DELTA] != N - 1) {
        fprintf(stderr, "delta_round: expected %u OFS_DELTA, got %u\n",
                (unsigned)(N - 1), counts[PACK_OBJ_OFS_DELTA]);
        fail(TESTFAIL);
    }
    //  (c) Delta-of-delta chaining actually happened, and runs deep.
    if (deltas_on_deltas == 0) {
        fprintf(stderr,
                "delta_round: no delta-of-delta — chains did not form\n");
        fail(TESTFAIL);
    }
    if (max_depth < 3) {
        fprintf(stderr,
                "delta_round: chain too shallow (max depth %u < 3)\n",
                max_depth);
        fail(TESTFAIL);
    }
    //  (d) Chaining must recover compression vs the all-raw fallback.
    if (u8csLen(logbytes) >= (u64)allraw_len) {
        fprintf(stderr,
                "delta_round: chained log %llu not smaller than "
                "all-raw %llu\n",
                (unsigned long long)u8csLen(logbytes),
                (unsigned long long)allraw_len);
        fail(TESTFAIL);
    }

    //  (e) KEEPGet byte-matches EVERY version through the OFS chain
    //  (the read resolver follows the delta-of-delta hops we just
    //  built).  Done before the git round-trip so a resolver regression
    //  is caught even if git's own delta apply happens to agree.
    {
        Bu8 gbuf = {};
        call(u8bMap, gbuf, 1 << 20);
        for (int i = 0; i < N; i++) {
            u8bReset(gbuf);
            u8 gt = 0;
            ok64 grc = KEEPGet(WHIFFHashlet60(&shas[i]), 15, gbuf, &gt);
            u64 wl = strlen(versions[i]);
            if (grc != OK || u8bDataLen(gbuf) != wl ||
                memcmp(u8bDataHead(gbuf), versions[i], wl) != 0) {
                fprintf(stderr,
                        "delta_round: KEEPGet v%d mismatch (rc ok=%d, "
                        "got %llu want %llu)\n",
                        i, grc == OK,
                        (unsigned long long)u8bDataLen(gbuf),
                        (unsigned long long)wl);
                u8bUnMap(gbuf);
                FILEUnMap(logmap);
                fail(TESTFAIL);
            }
        }
        u8bUnMap(gbuf);
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

//  GIT-008 depth cap: feed a chain longer than KEEP_WRITE_CHAIN_MAX
//  (50) and prove the writer bounds it — the deepest OFS chain must
//  never exceed the cap, and once the cap is hit a fresh RAW root must
//  appear (so the store has MORE than one raw blob).  This keeps reads
//  cheap and the MEM-022-class resolver bound far from being neared.
ok64 DELTACapBound() {
    sane(1);
    call(FILEInit);

    char tmp[] = "/tmp/keeper-delta-cap-XXXXXX";
    want(mkdtemp(tmp) != NULL);
    u8cs root = {(u8cp)tmp, (u8cp)tmp + strlen(tmp)};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);

    enum { M = 60 };                 // > KEEP_WRITE_CHAIN_MAX (50)
    enum { CAP = 50 };               // mirror of KEEP_WRITE_CHAIN_MAX
    static char vbuf[M][256];
    sha1 shas[M] = {};
    {
        keep_pack p = {};
        call(KEEPPackOpen, &p);
        p.strict_order = NO;
        for (int i = 0; i < M; i++) {
            snprintf(vbuf[i], sizeof(vbuf[i]),
                     "revision body line one, stable preamble text; "
                     "this is mutation number %05d trailing edit.", i);
            u8csc c = {(u8cp)vbuf[i], (u8cp)vbuf[i] + strlen(vbuf[i])};
            u64 hint = i == 0 ? 0 : WHIFFHashlet60(&shas[i-1]);
            call(KEEPPackFeed, &p, DOG_OBJ_BLOB, c, hint, &shas[i]);
        }
        call(KEEPPackClose, &p);
    }

    char logpath[1024];
    snprintf(logpath, sizeof(logpath),
             "%s/" DOG_BE_NAME "/0000000001.keeper", tmp);
    u8bp logmap = NULL;
    {
        u8cs lpp = {(u8cp)logpath, (u8cp)logpath + strlen(logpath)};
        a_pad(u8, lpbuf, 1100);
        u8bFeed(lpbuf, lpp);
        u8bFeed1(lpbuf, 0);
        u8cs lpt = {u8bDataHead(lpbuf),
                    u8bDataHead(lpbuf) + u8bDataLen(lpbuf) - 1};
        call(FILEMapRO, &logmap, lpt);
    }

    u32 counts[8] = {};
    a_dup(u8c, lb, u8bData(logmap));
    call(count_types, lb, counts);
    u32 dod = 0, max_depth = 0;
    {
        a_dup(u8c, lb2, u8bData(logmap));
        call(chain_stats, lb2, &dod, &max_depth);
    }
    fprintf(stderr,
            "delta_cap: blob=%u ofs=%u ref=%u max-depth=%u (cap=%d)\n",
            counts[PACK_OBJ_BLOB], counts[PACK_OBJ_OFS_DELTA],
            counts[PACK_OBJ_REF_DELTA], max_depth, CAP);

    //  Cap honored: deepest chain never exceeds the writer cap.
    if (max_depth > CAP) {
        fprintf(stderr, "delta_cap: chain depth %u exceeds cap %d\n",
                max_depth, CAP);
        FILEUnMap(logmap);
        fail(TESTFAIL);
    }
    //  Cap actually engaged: a fresh raw root appeared mid-stream, so
    //  there is more than one raw blob (the chain restarted).
    if (counts[PACK_OBJ_BLOB] < 2) {
        fprintf(stderr,
                "delta_cap: expected >=2 raw roots (cap restart), got %u\n",
                counts[PACK_OBJ_BLOB]);
        FILEUnMap(logmap);
        fail(TESTFAIL);
    }
    //  Still OFS-only.
    if (counts[PACK_OBJ_REF_DELTA] != 0) {
        fprintf(stderr, "delta_cap: %u REF_DELTA leaked\n",
                counts[PACK_OBJ_REF_DELTA]);
        FILEUnMap(logmap);
        fail(TESTFAIL);
    }

    //  Every version still resolves correctly (reads stay cheap and
    //  correct across the cap restart).
    {
        Bu8 gbuf = {};
        call(u8bMap, gbuf, 1 << 20);
        for (int i = 0; i < M; i++) {
            u8bReset(gbuf);
            u8 gt = 0;
            ok64 grc = KEEPGet(WHIFFHashlet60(&shas[i]), 15, gbuf, &gt);
            u64 wl = strlen(vbuf[i]);
            if (grc != OK || u8bDataLen(gbuf) != wl ||
                memcmp(u8bDataHead(gbuf), vbuf[i], wl) != 0) {
                fprintf(stderr, "delta_cap: KEEPGet v%d mismatch\n", i);
                u8bUnMap(gbuf);
                FILEUnMap(logmap);
                fail(TESTFAIL);
            }
        }
        u8bUnMap(gbuf);
    }

    FILEUnMap(logmap);
    call(KEEPClose);
    HOMEClose();
    char cmd[1100];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", tmp);
    system(cmd);
    done;
}

ok64 maintest() {
    sane(1);
    call(DELTARoundTrip);
    call(DELTACapBound);
    done;
}

TEST(maintest)
