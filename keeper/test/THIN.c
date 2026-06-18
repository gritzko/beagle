//  THIN (GIT-005): single-stream serving — verbatim whole-log full
//  clone + a real thin-pack builder for incremental fetch.
//
//  Strategy: build a real git repo with a LARGE blob (so deltas matter)
//  whose content changes by ONE line across a commit boundary, repack
//  it OFS-only, ingest the pack into a fresh keeper (the native store is
//  now one OFS-only `.keeper` log), then exercise the two serve arms:
//
//    (a) Full clone (no haves): WIREBuildSegments enumerates the
//        `.keeper` file(s) verbatim, PSTRWrite stitches a packfile, and
//        `git index-pack` accepts it (byte round-trip).
//    (b) Incremental fetch (have = the older commit, want = the newer):
//        WIREBuildThinPack walks the want's closure minus the have's,
//        re-frames deltas OFS/REF for the output pack, and the source
//        repo's `git index-pack --fix-thin` completes it — proving a
//        vanilla git client unpacks the thin pack.
//    (c) Efficiency: the thin pack for a 1-line change across the
//        frontier is SMALL (far smaller than the big blob) and the
//        changed blob is delta-encoded, NOT a raw re-ship.
//
#include "keeper/KEEP.h"
#include "keeper/WIRE.h"
#include "keeper/PSTR.h"
#include "keeper/REFADV.h"
#include "dog/git/PACK.h"
#include "dog/git/SHA1.h"
#include "dog/HOME.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PRO.h"
#include "abc/TEST.h"

#define GIT_UNSET "unset GIT_DIR GIT_WORK_TREE GIT_COMMON_DIR " \
                  "GIT_INDEX_FILE GIT_OBJECT_DIRECTORY && "

#define SH(fmt, ...) do {                                      \
    char _cmd[2048];                                           \
    int _n = snprintf(_cmd, sizeof(_cmd), fmt, ##__VA_ARGS__); \
    want(_n > 0 && _n < (int)sizeof(_cmd));                    \
    int _rc = system(_cmd);                                    \
    want(_rc == 0);                                            \
} while (0)

static void tmp_rm(char const *path) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    int _ = system(cmd);
    (void)_;
}

//  Capture first line of a command's stdout into `out` (NUL trimmed).
static ok64 sh_read(char const *cmd, char *out, size_t cap) {
    sane(cmd && out);
    FILE *p = popen(cmd, "r");
    if (!p) return FAIL;
    size_t n = fread(out, 1, cap - 1, p);
    out[n] = 0;
    pclose(p);
    while (n && (out[n - 1] == '\n' || out[n - 1] == ' ')) out[--n] = 0;
    done;
}

//  Locate the single .pack file under <repo>/.git/objects/pack/.
static ok64 find_pack(char const *repo, char *path_out, size_t cap) {
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.git/objects/pack", repo);
    DIR *d = opendir(dir);
    if (!d) return FAIL;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(d)) != NULL) {
        size_t ln = strlen(e->d_name);
        if (ln > 5 && strcmp(e->d_name + ln - 5, ".pack") == 0) {
            snprintf(path_out, cap, "%s/%s", dir, e->d_name);
            found = 1;
            break;
        }
    }
    closedir(d);
    return found ? OK : FAIL;
}

//  Slurp a pack file at `path`, ingest into the OPEN keeper.
static ok64 ingest_pack_file(char const *path) {
    sane(path);
    static u8 pbuf[1 << 24];
    FILE *f = fopen(path, "rb");
    if (!f) return FAIL;
    size_t plen = fread(pbuf, 1, sizeof(pbuf), f);
    fclose(f);
    if (plen == 0 || plen >= sizeof(pbuf)) return FAIL;
    u8csc bytes = {pbuf, pbuf + plen};
    return KEEPIngestFile(bytes);
}

//  Build a repo at `repo`: a LARGE blob (`BIGN` numbered lines) committed
//  (commit B), then one line changed and committed (commit H).  Repack
//  OFS-only.  Returns commit shas B and H (40-hex + NUL) and the size of
//  the .pack.
#define BIGN 4000
static ok64 stage_repo(char const *repo, char *b_hex, char *h_hex) {
    sane(repo && b_hex && h_hex);
    SH(GIT_UNSET "cd %s && git init -q && "
       "git config user.email t@t && git config user.name t && "
       "seq 1 %d > big.txt && "
       "git add big.txt && git commit -q -m base && "
       "sed -i '2000s/.*/CHANGED LINE/' big.txt && "
       "git add big.txt && git commit -q -m tip && "
       "git repack -q -Ad", repo, BIGN);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             GIT_UNSET "cd %s && git rev-parse HEAD~1", repo);
    call(sh_read, cmd, b_hex, 64);
    snprintf(cmd, sizeof(cmd),
             GIT_UNSET "cd %s && git rev-parse HEAD", repo);
    call(sh_read, cmd, h_hex, 64);
    done;
}

//  Stand up a fresh keeper at `kdir` holding `repo`'s pack.  Opens HOME
//  + KEEP rw, ingests, closes.  Leaves them CLOSED — caller reopens.
static ok64 stage_keeper(char const *kdir, char const *repo) {
    sane(kdir && repo);
    a_cstr(root_s, kdir);
    home h = {};
    call(HOMEOpenAt, root_s, YES);
    call(KEEPOpen, YES);
    char pack_path[1024];
    call(find_pack, repo, pack_path, sizeof(pack_path));
    call(ingest_pack_file, pack_path);
    call(KEEPClose);
    HOMEClose();
    done;
}

//  Run `git index-pack` on a candidate pack at `path`.  When `repo` is
//  non-NULL, run it INSIDE that repo with --fix-thin so a thin pack's
//  external bases resolve against the repo's object store; else a plain
//  --stdin index of a self-contained pack.  Returns OK iff git accepts.
static ok64 git_index_pack(char const *path, char const *repo, b8 thin) {
    sane(path);
    char cmd[2048];
    if (repo) {
        snprintf(cmd, sizeof(cmd),
                 GIT_UNSET "cd %s && git index-pack %s --stdin < %s "
                 ">/dev/null 2>&1",
                 repo, thin ? "--fix-thin" : "", path);
    } else {
        char idx[1100];
        snprintf(idx, sizeof(idx), "%s.idx", path);
        snprintf(cmd, sizeof(cmd),
                 GIT_UNSET "cd / && git index-pack -o %s %s >/dev/null 2>&1",
                 idx, path);
    }
    int rc = system(cmd);
    if (repo) {
        char rm[1200];
        snprintf(rm, sizeof(rm), "rm -f %s.idx %s.pack", path, path);
        int _ = system(rm); (void)_;
    } else {
        char rm[1200];
        snprintf(rm, sizeof(rm), "%s.idx", path);
        unlink(rm);
    }
    return rc == 0 ? OK : FAIL;
}

//  Parse a packfile's header count + walk every object record header,
//  reporting how many are delta records (OFS or REF) and the total
//  uncompressed... no — we only need the record TYPES.  Returns the
//  header count via *count and YES via *any_raw_big if any RAW record's
//  declared size exceeds `big_threshold` (a re-shipped large blob).
static ok64 pack_inspect(u8csc pack, u32 *count, u32 *ndelta,
                         b8 *raw_big, u64 big_threshold) {
    sane(count && ndelta && raw_big);
    *ndelta = 0; *raw_big = NO;
    a_dup(u8c, scan, pack);
    pack_hdr h = {};
    call(PACKDrainHdr, scan, &h);
    *count = h.count;
    for (u32 i = 0; i < h.count; i++) {
        pack_obj o = {};
        call(PACKDrainObjHdr, scan, &o);
        if (o.type == PACK_OBJ_OFS_DELTA || o.type == PACK_OBJ_REF_DELTA) {
            (*ndelta)++;
            //  Skip the deflated delta payload: inflate-skip by letting
            //  zlib consume it (we re-inflate to advance the cursor).
            a_carve(u8, tmp, 1ULL << 24);
            u8s into = {u8bIdleHead(tmp), u8bIdleHead(tmp) + u8bIdleLen(tmp)};
            call(PACKInflate, scan, into, o.size);
        } else {
            if (o.size > big_threshold) *raw_big = YES;
            a_carve(u8, tmp, 1ULL << 24);
            u8s into = {u8bIdleHead(tmp), u8bIdleHead(tmp) + u8bIdleLen(tmp)};
            call(PACKInflate, scan, into, o.size);
        }
    }
    done;
}

// ---- Test (a): full clone byte round-trip via PSTRWrite ----

ok64 THINtest_full_clone() {
    sane(1);
    call(FILEInit);

    char repo[] = "/tmp/thin-repo-a-XXXXXX";
    want(mkdtemp(repo) != NULL);
    char b_hex[64], h_hex[64];
    call(stage_repo, repo, b_hex, h_hex);

    char kdir[] = "/tmp/thin-keep-a-XXXXXX";
    want(mkdtemp(kdir) != NULL);
    call(stage_keeper, kdir, repo);

    //  Reopen the keeper and build the full-clone segment list for a
    //  want = the tip commit, no haves.
    a_cstr(root_s, kdir);
    home hh = {};
    call(HOMEOpenAt, root_s, YES);
    call(KEEPOpen, YES);
    a_refadv(adv);
    call(REFADVOpen, &adv);

    sha1 want = {};
    {
        u8cs hs = {(u8 *)h_hex, (u8 *)h_hex + 40};
        call(sha1FromHex, &want, hs);
    }

    wire_req req = {};
    req.nwants = 1;
    req.wants[0] = want;

    pstr_seg segs[WIRE_MAX_WANTS];
    int      fds [WIRE_MAX_WANTS];
    for (u32 i = 0; i < WIRE_MAX_WANTS; i++) fds[i] = -1;
    u32 nseg = 0;
    call(WIREBuildSegments, &adv, &req, segs, fds, WIRE_MAX_WANTS, &nseg);
    want(nseg >= 1);

    //  Stitch + index-pack.
    char outpath[] = "/tmp/thin-full-XXXXXX";
    int out_fd = mkstemp(outpath);
    want(out_fd >= 0);
    pstr_segcs slice = {segs, segs + nseg};
    call(PSTRWrite, out_fd, slice);
    close(out_fd);
    for (u32 i = 0; i < WIRE_MAX_WANTS; i++) if (fds[i] >= 0) close(fds[i]);

    //  Self-contained full pack: plain index-pack (no repo).
    call(git_index_pack, outpath, NULL, NO);
    unlink(outpath);

    REFADVClose(&adv);
    call(KEEPClose);
    HOMEClose();
    tmp_rm(repo);
    tmp_rm(kdir);
    done;
}

// ---- Tests (b)+(c): incremental thin pack across a delta boundary ----

ok64 THINtest_incremental_thin() {
    sane(1);
    call(FILEInit);

    char repo[] = "/tmp/thin-repo-b-XXXXXX";
    want(mkdtemp(repo) != NULL);
    char b_hex[64], h_hex[64];
    call(stage_repo, repo, b_hex, h_hex);

    char kdir[] = "/tmp/thin-keep-b-XXXXXX";
    want(mkdtemp(kdir) != NULL);
    call(stage_keeper, kdir, repo);

    a_cstr(root_s, kdir);
    home hh = {};
    call(HOMEOpenAt, root_s, YES);
    call(KEEPOpen, YES);
    a_refadv(adv);
    call(REFADVOpen, &adv);

    sha1 want = {}, have = {};
    {
        u8cs hs = {(u8 *)h_hex, (u8 *)h_hex + 40};
        call(sha1FromHex, &want, hs);
        u8cs bs = {(u8 *)b_hex, (u8 *)b_hex + 40};
        call(sha1FromHex, &have, bs);
    }

    wire_req req = {};
    req.nwants = 1;  req.wants[0] = want;
    req.nhaves = 1;  req.haves[0] = have;
    req.caps   = WIRE_CAP_OFS_DELTA;

    Bu8 packbuf = {};
    call(u8bAllocate, packbuf, 1ULL << 26);
    call(WIREBuildThinPack, &adv, &req, packbuf);

    a_dup(u8c, pack, u8bData(packbuf));
    u64 packlen = (u64)u8csLen(pack);

    //  (c) Efficiency: the whole big blob is ~ 5*BIGN bytes raw; a
    //  1-line-change thin pack must be a tiny fraction of that.  Assert
    //  the pack is far smaller than the raw big blob, and that NO raw
    //  record in it re-ships the big blob (every changed object is
    //  delta-encoded against the frontier).
    u32 count = 0, ndelta = 0;
    b8  raw_big = NO;
    call(pack_inspect, pack, &count, &ndelta, &raw_big, 4096);
    fprintf(stderr,
            "THIN: incremental pack=%llu bytes, count=%u, ndelta=%u, "
            "raw_big=%d\n",
            (unsigned long long)packlen, count, ndelta, (int)raw_big);
    //  The changed file's blob must be delta-encoded across the frontier.
    want(ndelta >= 1);
    //  No raw re-ship of the (~20 KiB) big blob.
    want(raw_big == NO);
    //  Small: well under the big blob's raw size (~5*BIGN ≈ 19 KiB).
    want(packlen < 8000);
    //  The pack must be the want's closure minus the have's: only the
    //  new commit + new tree + changed blob (3 objects) — the unchanged
    //  blob and shared subtrees are pruned.
    want(count == 3);

    //  (b) A vanilla git client unpacks it: write the thin pack and
    //  complete it with `git index-pack --fix-thin` inside the source
    //  repo (whose object store holds the external bases).
    char outpath[] = "/tmp/thin-inc-XXXXXX";
    int out_fd = mkstemp(outpath);
    want(out_fd >= 0);
    want(write(out_fd, pack[0], (size_t)packlen) == (ssize_t)packlen);
    close(out_fd);
    call(git_index_pack, outpath, repo, YES);
    unlink(outpath);

    u8bFree(packbuf);
    REFADVClose(&adv);
    call(KEEPClose);
    HOMEClose();
    tmp_rm(repo);
    tmp_rm(kdir);
    done;
}

//  ---- Test (d): no-ofs-delta cap → REF_DELTA, still unpacks ----

ok64 THINtest_no_ofs_cap() {
    sane(1);
    call(FILEInit);

    char repo[] = "/tmp/thin-repo-d-XXXXXX";
    want(mkdtemp(repo) != NULL);
    char b_hex[64], h_hex[64];
    call(stage_repo, repo, b_hex, h_hex);

    char kdir[] = "/tmp/thin-keep-d-XXXXXX";
    want(mkdtemp(kdir) != NULL);
    call(stage_keeper, kdir, repo);

    a_cstr(root_s, kdir);
    home hh = {};
    call(HOMEOpenAt, root_s, YES);
    call(KEEPOpen, YES);
    a_refadv(adv);
    call(REFADVOpen, &adv);

    sha1 want = {}, have = {};
    {
        u8cs hs = {(u8 *)h_hex, (u8 *)h_hex + 40};
        call(sha1FromHex, &want, hs);
        u8cs bs = {(u8 *)b_hex, (u8 *)b_hex + 40};
        call(sha1FromHex, &have, bs);
    }

    wire_req req = {};
    req.nwants = 1;  req.wants[0] = want;
    req.nhaves = 1;  req.haves[0] = have;
    req.caps   = 0;   //  NO ofs-delta negotiated → every delta REF_DELTA

    Bu8 packbuf = {};
    call(u8bAllocate, packbuf, 1ULL << 26);
    call(WIREBuildThinPack, &adv, &req, packbuf);

    a_dup(u8c, pack, u8bData(packbuf));
    u64 packlen = (u64)u8csLen(pack);

    //  Still a small thin pack (REF_DELTA against the frontier blob).
    want(packlen < 8000);

    char outpath[] = "/tmp/thin-noofs-XXXXXX";
    int out_fd = mkstemp(outpath);
    want(out_fd >= 0);
    want(write(out_fd, pack[0], (size_t)packlen) == (ssize_t)packlen);
    close(out_fd);
    call(git_index_pack, outpath, repo, YES);
    unlink(outpath);

    u8bFree(packbuf);
    REFADVClose(&adv);
    call(KEEPClose);
    HOMEClose();
    tmp_rm(repo);
    tmp_rm(kdir);
    done;
}

ok64 maintest() {
    sane(1);
    fprintf(stderr, "THINtest_full_clone...\n");
    call(THINtest_full_clone);
    fprintf(stderr, "THINtest_incremental_thin...\n");
    call(THINtest_incremental_thin);
    fprintf(stderr, "THINtest_no_ofs_cap...\n");
    call(THINtest_no_ofs_cap);
    fprintf(stderr, "all passed\n");
    done;
}

TEST(maintest)
