//  UNPK tests: run UNPKIndex against packfiles produced by real git.
//
//  Strategy: shell out to `git` to build tiny toy repos in mkdtemp-created
//  directories, `git repack -Ad` to collapse into a single pack, mmap it,
//  run UNPKIndex, cross-check the emitted wh128 hashlets against the
//  objects `git cat-file --batch-all-objects --batch-check` lists.
//
#include "keeper/KEEP.h"
#include "keeper/UNPK.h"
#include "dog/git/PACK.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PRO.h"
#include "abc/TEST.h"
#include "dog/WHIFF.h"

// Run a shell command; want() success.
#define SH(fmt, ...) do {                                      \
    char _cmd[1024];                                           \
    int _n = snprintf(_cmd, sizeof(_cmd), fmt, ##__VA_ARGS__); \
    want(_n > 0 && _n < (int)sizeof(_cmd));                    \
    int _rc = system(_cmd);                                    \
    want(_rc == 0);                                            \
} while (0)

// Capture first line of a command's stdout into `out` (NUL terminated).
static ok64 sh_read(char const *cmd, char *out, size_t cap) {
    FILE *p = popen(cmd, "r");
    if (!p) return UNPKFAIL;
    size_t n = fread(out, 1, cap - 1, p);
    out[n] = 0;
    pclose(p);
    return OK;
}

// Locate the single .pack file under <repo>/.git/objects/pack/.
// Writes a NUL-terminated path into `path_out` (cap bytes).
static ok64 find_pack(char const *repo, char *path_out, size_t cap) {
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.git/objects/pack", repo);
    DIR *d = opendir(dir);
    if (!d) return UNPKFAIL;
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
    return found ? OK : UNPKFAIL;
}

// Prefix to strip inherited GIT_* env vars from shell subcommands.
#define GIT_UNSET "unset GIT_DIR GIT_WORK_TREE GIT_COMMON_DIR " \
                   "GIT_INDEX_FILE GIT_OBJECT_DIRECTORY && "

// Count objects in a repo via `git cat-file --batch-all-objects --batch-check=%(objectname)`.
static ok64 count_objects(char const *repo, u32 *out_count) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             GIT_UNSET "cd %s && git cat-file --batch-all-objects "
             "--batch-check='%%(objectname)' | wc -l", repo);
    char buf[64];
    if (sh_read(cmd, buf, sizeof(buf)) != OK) return UNPKFAIL;
    *out_count = (u32)strtoul(buf, NULL, 10);
    return OK;
}

// Pull SHA-1 hex of every object into `out` as a 40-byte-stride string array.
// Returns count.
static ok64 list_shas(char const *repo, u8 *out, size_t cap, u32 *out_count) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             GIT_UNSET "cd %s && git cat-file --batch-all-objects "
             "--batch-check='%%(objectname)'", repo);
    FILE *p = popen(cmd, "r");
    if (!p) return UNPKFAIL;
    u32 n = 0;
    char line[128];
    while (fgets(line, sizeof(line), p)) {
        if (strlen(line) < 40) continue;
        if ((size_t)n * 40 + 40 > cap) break;
        memcpy(out + (size_t)n * 40, line, 40);
        n++;
    }
    pclose(p);
    *out_count = n;
    return OK;
}

// Compute the hashlet60 each SHA-1 hex would produce.
// Returns sorted array of expected hashlets.
static void expected_hashlets(u8 const *shas, u32 n, u64 *out) {
    for (u32 i = 0; i < n; i++) {
        sha1 s = {};
        for (int j = 0; j < 20; j++) {
            u8 hi = shas[(size_t)i * 40 + j * 2];
            u8 lo = shas[(size_t)i * 40 + j * 2 + 1];
            #define NIB(c) ((c) >= '0' && (c) <= '9' ? (c) - '0' :     \
                             (c) >= 'a' && (c) <= 'f' ? (c) - 'a' + 10 : \
                             (c) >= 'A' && (c) <= 'F' ? (c) - 'A' + 10 : 0)
            s.data[j] = (u8)((NIB(hi) << 4) | NIB(lo));
            #undef NIB
        }
        out[i] = WHIFFHashlet60(&s);
    }
    // insertion sort (n is small)
    for (u32 i = 1; i < n; i++) {
        u64 v = out[i];
        u32 j = i;
        while (j > 0 && out[j - 1] > v) { out[j] = out[j - 1]; j--; }
        out[j] = v;
    }
}

// Binary search for a hashlet in a sorted array.
static int has_hashlet(u64 const *sorted, u32 n, u64 h) {
    u32 lo = 0, hi = n;
    while (lo < hi) {
        u32 mid = lo + (hi - lo) / 2;
        if (sorted[mid] < h) lo = mid + 1;
        else hi = mid;
    }
    return lo < n && sorted[lo] == h;
}

// --- emit-callback test helpers -----------------------------------

// Count emitted objects per type.  Path derivation is no longer
// performed by UNPK — naming is owned by spot's Close-pass tree walk.
typedef struct {
    u32 nblobs;
    u32 ntrees;
    u32 ncommits;
} emit_collect;

static void emit_cb(void *ctx, u8 type, sha1cp sha,
                     u8cs content) {
    (void)sha; (void)content;
    emit_collect *c = (emit_collect *)ctx;
    if (type == PACK_OBJ_COMMIT) c->ncommits++;
    else if (type == PACK_OBJ_TREE) c->ntrees++;
    else if (type == PACK_OBJ_BLOB) c->nblobs++;
}

// Common setup: mkdtemp for both toy repo and keeper root.
// Open keeper, run a toy-repo shell recipe, mmap the pack, call UNPKIndex,
// verify emitted hashlets cover every object git reports.
static ok64 run_toy(char const *recipe) {
    sane(1);
    call(FILEInit);

    char repo[] = "/tmp/unpk-repo-XXXXXX";
    want(mkdtemp(repo) != NULL);
    char kdir[] = "/tmp/unpk-keep-XXXXXX";
    want(mkdtemp(kdir) != NULL);

    // Build the toy repo via shell recipe.
    // The recipe is a printf-format string with one %s slot for the repo path.
    // Isolate from any inherited GIT_* env vars (test runs inside a worktree).
    char rcmd[4096];
    int n = snprintf(rcmd, sizeof(rcmd), GIT_UNSET);
    n += snprintf(rcmd + n, sizeof(rcmd) - n, recipe, repo);
    want(n > 0 && n < (int)sizeof(rcmd));
    want(system(rcmd) == 0);

    // Find the .pack file.
    char pack_path[1024];
    call(find_pack, repo, pack_path, sizeof(pack_path));

    // mmap the pack.
    u8bp pack_map = NULL;
    a_cstr(pp, "");  // placeholder
    (void)pp;
    u8cs pp_slice = {(u8cp)pack_path, (u8cp)pack_path + strlen(pack_path)};
    a_path(pack_p8, pp_slice);
    call(FILEMapRO, &pack_map, $path(pack_p8));
    u8cp pbase = u8bDataHead(pack_map);
    u64 plen = (u64)(u8bIdleHead(pack_map) - pbase);

    // Parse header.
    u8cs scan = {pbase, pbase + plen};
    pack_hdr hdr = {};
    call(PACKDrainHdr, scan, &hdr);
    want(hdr.version == 2);

    // Set up keeper against kdir.
    u8cs root = {(u8cp)kdir, (u8cp)kdir + strlen(kdir)};
    home h = {};
    call(HOMEOpenAt, root, YES);
    
    call(KEEPOpen, YES);

    //  Emit callback: count objects per type.
    emit_collect ec = {};

    // Run UNPKIndex against the whole pack (log_off == pack_off, file_id arbitrary).
    unpk_in in = {
        .pack = {pbase, pbase + plen},
        .scan_start = 12,
        .scan_end = plen >= 20 ? plen - 20 : plen,
        .count = hdr.count,
        .file_id = 1,
        .emit = emit_cb,
        .emit_ctx = &ec,
    };

    Bwh128 entries = {};
    call(wh128bAllocate, entries, hdr.count ? hdr.count * 2 : 16);
    unpk_stats st = {};
    call(UNPKIndex, &in, entries, &st);

    //  Per-event sanity: emit counts match object types in the pack.
    want(ec.nblobs + ec.ntrees + ec.ncommits <= hdr.count);

    // Compare to git's object list.
    u32 expected_n = 0;
    call(count_objects, repo, &expected_n);
    want(expected_n == hdr.count);

    u8 *shas = malloc((size_t)expected_n * 40);
    want(shas != NULL);
    u32 listed_n = 0;
    call(list_shas, repo, shas, (size_t)expected_n * 40, &listed_n);
    want(listed_n == expected_n);

    u64 *expected = malloc((size_t)expected_n * sizeof(u64));
    want(expected != NULL);
    expected_hashlets(shas, expected_n, expected);

    // Every indexed entry's hashlet must appear in the expected set.
    a_dup(wh128c, emitted, wh128bDataC(entries));
    u32 emitted_n = (u32)(emitted[1] - emitted[0]);
    want(emitted_n == st.indexed);
    want(st.indexed == hdr.count);
    want(st.skipped == 0);

    for (u32 i = 0; i < emitted_n; i++) {
        u64 h = keepKeyHashlet(emitted[0][i].key);
        want(has_hashlet(expected, expected_n, h));
        u8 type = keepKeyType(emitted[0][i].key);
        want(type >= 1 && type <= 4);
    }

    free(expected);
    free(shas);
    wh128bFree(entries);
    KEEPClose();
    HOMEClose();
    FILEUnMap(pack_map);

    SH("rm -rf %s", repo);
    SH("rm -rf %s", kdir);

    done;
}

// --- Test 1: single commit, one file ---
//
// Produces 3 objects: one blob, one tree, one commit — all bases.
ok64 UNPKtest_single() {
    return run_toy(
        "cd %s && "
        "git init -q && "
        "git config user.email t@t && git config user.name t && "
        "printf 'hello\\n' > a.txt && "
        "git add a.txt && git commit -q -m first && "
        "git repack -q -Ad"
    );
}

// --- Test 2: multiple commits touching same file ---
//
// Several versions of a.txt — `git repack -Ad` will produce OFS_DELTAs
// between blob versions, plus bases for commits and trees.
ok64 UNPKtest_delta_chain() {
    return run_toy(
        "cd %s && "
        "git init -q && "
        "git config user.email t@t && git config user.name t && "
        "for i in 1 2 3 4 5 6 7 8 9 10; do "
        "  printf 'line %%d\\n' $(seq 1 $i) > a.txt && "
        "  git add a.txt && "
        "  git commit -q -m \"c$i\"; "
        "done && "
        "git repack -q -Ad"
    );
}

// --- Test 3: multiple files, multiple commits ---
//
// Forces tree-level deltas too.
ok64 UNPKtest_mixed() {
    return run_toy(
        "cd %s && "
        "git init -q && "
        "git config user.email t@t && git config user.name t && "
        "for i in 1 2 3 4 5; do "
        "  mkdir -p dir$i && "
        "  for j in 1 2 3; do printf 'f%%d line\\n' $(seq 1 $((i+j))) > dir$i/f$j.txt; done && "
        "  git add . && git commit -q -m \"c$i\"; "
        "done && "
        "git repack -q -Ad"
    );
}

// === GIT-003: ingest re-encoder — foreign pack → OFS-only log ========
//
// KEEPIngestFile must normalize ANY foreign pack (OFS, REF_DELTA, thin)
// into the shard's single OFS-only stream: zero REF_DELTA records,
// dedup'd objects, every object byte-for-byte recoverable via KEEPGet.
// These tests drive a real git pack (built in a chosen delta scheme)
// through KEEPIngestFile, then (1) scan every `.keeper` log file and
// count records by pack type, and (2) round-trip each git object.

//  Walk one keeper log file's record stream from offset 12 (after the
//  single file-level PACK header) for `count` objects, tallying how
//  many are REF_DELTA / OFS_DELTA / raw.  Mirrors UNPK's pre-scan: each
//  record is a header varint then deflated bytes; inflate into scratch
//  purely to advance the cursor.
static ok64 scan_log_types(u8cs log, u32 count, u32 *n_ref,
                           u32 *n_ofs, u32 *n_raw) {
    sane(u8csOK(log));
    u8cs scan = {log[0], log[1]};
    if (u8csLen(scan) < 12) return UNPKFAIL;
    u8csUsed(scan, 12);                  // skip file-level PACK header
    static u8 inflate_scratch[1 << 22];
    for (u32 i = 0; i < count; i++) {
        pack_obj obj = {};
        call(PACKDrainObjHdr, scan, &obj);
        if (obj.type == PACK_OBJ_REF_DELTA) (*n_ref)++;
        else if (obj.type == PACK_OBJ_OFS_DELTA) (*n_ofs)++;
        else (*n_raw)++;
        u8s into = {inflate_scratch, inflate_scratch + sizeof(inflate_scratch)};
        call(PACKInflate, scan, into, obj.size);
    }
    done;
}

//  Read the file-level object count (big-endian u32 at byte 8) from a
//  keeper log's PACK header.
static u32 log_header_count(u8cs log) {
    u8cp h = log[0];
    return ((u32)h[8] << 24) | ((u32)h[9] << 16) |
           ((u32)h[10] << 8) | (u32)h[11];
}

//  Tally REF/OFS/raw records across every `.keeper` file in the shard
//  dir `<kdir>/.be/<project>/`.  Returns the project shard dir's path
//  via popen-glob (one shard per project, GIT-001).
static ok64 shard_log_tally(char const *kdir, u32 *n_ref, u32 *n_ofs,
                            u32 *n_raw, u32 *n_files) {
    sane(kdir);
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "find %s/.be -name '*.keeper' -type f 2>/dev/null", kdir);
    FILE *p = popen(cmd, "r");
    if (!p) return UNPKFAIL;
    char line[1024];
    while (fgets(line, sizeof(line), p)) {
        size_t ln = strlen(line);
        while (ln && (line[ln - 1] == '\n' || line[ln - 1] == ' ')) line[--ln] = 0;
        if (ln == 0) continue;
        u8bp map = NULL;
        u8cs pth = {(u8cp)line, (u8cp)line + ln};
        a_path(p8, pth);
        if (FILEMapRO(&map, $path(p8)) != OK) { pclose(p); return UNPKFAIL; }
        u8cs log = {u8bDataHead(map), u8bIdleHead(map)};
        u32 cnt = log_header_count(log);
        ok64 so = scan_log_types(log, cnt, n_ref, n_ofs, n_raw);
        FILEUnMap(map);
        if (so != OK) { pclose(p); return so; }
        (*n_files)++;
    }
    pclose(p);
    done;
}

//  Slurp a pack file at `path`, ingest it into the OPEN keeper via
//  KEEPIngestFile.  Caller has HOME + KEEP open.
static ok64 ingest_pack_file(char const *path) {
    sane(path);
    static u8 pbuf[1 << 22];
    size_t plen = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return UNPKFAIL;
    plen = fread(pbuf, 1, sizeof(pbuf), f);
    fclose(f);
    if (plen == 0 || plen >= sizeof(pbuf)) return UNPKFAIL;
    u8csc bytes = {pbuf, pbuf + plen};
    return KEEPIngestFile(bytes);
}

//  Assert the OFS-only invariant + per-object round-trip for a keeper
//  at `kdir` that should hold exactly `repo`'s objects: (1) zero
//  REF_DELTA records across every `.keeper`; (2) every git object
//  recovers via KEEPGetExact (own SHA-1 re-verify).
static ok64 check_ofs_only_and_roundtrip(char const *repo,
                                         char const *kdir) {
    sane(repo && kdir);

    u32 expected_n = 0;
    call(count_objects, repo, &expected_n);
    u8 *shas = malloc((size_t)expected_n * 40);
    want(shas != NULL);
    u32 listed_n = 0;
    call(list_shas, repo, shas, (size_t)expected_n * 40, &listed_n);
    want(listed_n == expected_n);

    u32 n_ref = 0, n_ofs = 0, n_raw = 0, n_files = 0;
    call(shard_log_tally, kdir, &n_ref, &n_ofs, &n_raw, &n_files);
    want(n_ref == 0);                         // OFS-only invariant
    want(n_ofs + n_raw == expected_n);        // every object stored once

    for (u32 i = 0; i < expected_n; i++) {
        sha1 s = {};
        for (int j = 0; j < 20; j++) {
            u8 hi = shas[(size_t)i * 40 + j * 2];
            u8 lo = shas[(size_t)i * 40 + j * 2 + 1];
            #define NIB(c) ((c) >= '0' && (c) <= '9' ? (c) - '0' :     \
                             (c) >= 'a' && (c) <= 'f' ? (c) - 'a' + 10 : \
                             (c) >= 'A' && (c) <= 'F' ? (c) - 'A' + 10 : 0)
            s.data[j] = (u8)((NIB(hi) << 4) | NIB(lo));
            #undef NIB
        }
        Bu8 ob = {};
        call(u8bMap, ob, 1UL << 24);
        u8 ty = 0;
        ok64 go = KEEPGetExact(&s, ob, &ty);
        want(go == OK);
        want(ty >= 1 && ty <= 4);
        u8bUnMap(ob);
    }
    free(shas);
    done;
}

//  A larger, slowly-mutating file forces git to form real deltas
//  (blob + tree + commit chains) so the foreign packs actually carry
//  OFS/REF deltas to re-encode — tiny toy repos pack to all-raw.
#define INGEST_RECIPE \
    "cd %s && git init -q && " \
    "git config user.email t@t && git config user.name t && " \
    "base=$(seq 1 300) && " \
    "for i in $(seq 1 12); do " \
    "  { echo \"$base\"; printf 'rev %%d\\n' $i; } > big.txt && " \
    "  printf 'side %%d\\n' $(seq 1 $i) > side.txt && " \
    "  git add . && git commit -q -m \"c$i\"; " \
    "done"

//  Build the recipe repo at a fresh tmp dir; return paths via out bufs.
static ok64 build_ingest_repo(char *repo, char *kdir) {
    sane(repo);
    char rcmd[4096];
    int n = snprintf(rcmd, sizeof(rcmd), GIT_UNSET);
    n += snprintf(rcmd + n, sizeof(rcmd) - n, INGEST_RECIPE, repo);
    want(n > 0 && n < (int)sizeof(rcmd));
    want(system(rcmd) == 0);
    (void)kdir;
    done;
}

//  (a) Non-thin OFS pack → OFS-only log, dedup on re-ingest.  Repack
//  into ONE pack (git's default OFS deltas), ingest, assert OFS-only +
//  round-trip, then re-ingest and assert zero log growth (dedup).
ok64 UNPKtest_ingest_ofs() {
    sane(1);
    call(FILEInit);
    char repo[] = "/tmp/unpk-ofs-repo-XXXXXX";
    want(mkdtemp(repo) != NULL);
    char kdir[] = "/tmp/unpk-ofs-keep-XXXXXX";
    want(mkdtemp(kdir) != NULL);
    call(build_ingest_repo, repo, kdir);
    SH(GIT_UNSET "cd %s && git repack -q -Ad", repo);

    char pack_path[1024];
    call(find_pack, repo, pack_path, sizeof(pack_path));

    u8cs root = {(u8cp)kdir, (u8cp)kdir + strlen(kdir)};
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);
    call(ingest_pack_file, pack_path);
    call(check_ofs_only_and_roundtrip, repo, kdir);

    //  Dedup: re-ingest the same pack — no new records.
    u32 a_ref = 0, a_ofs = 0, a_raw = 0, a_f = 0;
    call(shard_log_tally, kdir, &a_ref, &a_ofs, &a_raw, &a_f);
    call(ingest_pack_file, pack_path);
    u32 b_ref = 0, b_ofs = 0, b_raw = 0, b_f = 0;
    call(shard_log_tally, kdir, &b_ref, &b_ofs, &b_raw, &b_f);
    want(b_ref == 0);
    want(b_ofs + b_raw == a_ofs + a_raw);     // zero growth on re-ingest
    KEEPClose();
    HOMEClose();

    SH("rm -rf %s", repo);
    SH("rm -rf %s", kdir);
    done;
}

//  (b) Thin REF_DELTA pack whose base is already stored.  Stage 1
//  ingests a base pack of the lower history (objects below a boundary
//  commit).  Stage 2 ingests a `git pack-objects --thin` pack of the
//  top range whose REF_DELTA bases sit BELOW the boundary — not in the
//  thin pack, resolved via the keeper (UNPK Phase C).  Both packs carry
//  REF_DELTA (no --delta-base-offset); the stored log must end OFS-only
//  with every object round-tripping.
ok64 UNPKtest_ingest_thin() {
    sane(1);
    call(FILEInit);
    char repo[] = "/tmp/unpk-thin-repo-XXXXXX";
    want(mkdtemp(repo) != NULL);
    char kdir[] = "/tmp/unpk-thin-keep-XXXXXX";
    want(mkdtemp(kdir) != NULL);
    char base_pack[] = "/tmp/unpk-thin-base-XXXXXX";
    char thin_pack[] = "/tmp/unpk-thin-thin-XXXXXX";
    { int fd = mkstemp(base_pack); want(fd >= 0); close(fd); }
    { int fd = mkstemp(thin_pack); want(fd >= 0); close(fd); }

    call(build_ingest_repo, repo, kdir);

    //  Boundary = HEAD~1.  Base pack = the whole history EXCEPT the tip
    //  commit (REF deltas form within it).  Thin pack = the tip commit
    //  alone with the boundary closure excluded, so its REF_DELTA bases
    //  (the prior blob/tree) live ONLY in the base pack — single-level
    //  thin deltas resolved via the keeper (UNPK Phase C), no in-pack
    //  delta-of-a-delta chains.
    SH(GIT_UNSET "cd %s && B=$(git rev-parse HEAD~1) && "
       "git pack-objects --stdout --revs <<EOF > %s\n$B\nEOF",
       repo, base_pack);
    SH(GIT_UNSET "cd %s && B=$(git rev-parse HEAD~1) && "
       "H=$(git rev-parse HEAD) && "
       "git pack-objects --thin --stdout --revs <<EOF > %s\n^$B\n$H\nEOF",
       repo, thin_pack);

    u8cs root = {(u8cp)kdir, (u8cp)kdir + strlen(kdir)};
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);
    call(ingest_pack_file, base_pack);      // stage 1: bases land
    call(ingest_pack_file, thin_pack);      // stage 2: thin → Phase C
    call(check_ofs_only_and_roundtrip, repo, kdir);
    KEEPClose();
    HOMEClose();

    SH("rm -f %s %s", base_pack, thin_pack);
    SH("rm -rf %s", repo);
    SH("rm -rf %s", kdir);
    done;
}

//  (c) Overlapping incremental fetch: two packs that share objects.
//  Pack-1 = lower history, pack-2 = full history (overlaps pack-1).
//  After both ingests the store holds each object exactly once
//  (dedup), OFS-only, round-tripping.
ok64 UNPKtest_ingest_dedup() {
    sane(1);
    call(FILEInit);
    char repo[] = "/tmp/unpk-dup-repo-XXXXXX";
    want(mkdtemp(repo) != NULL);
    char kdir[] = "/tmp/unpk-dup-keep-XXXXXX";
    want(mkdtemp(kdir) != NULL);
    char p1[] = "/tmp/unpk-dup-p1-XXXXXX";
    char p2[] = "/tmp/unpk-dup-p2-XXXXXX";
    { int fd = mkstemp(p1); want(fd >= 0); close(fd); }
    { int fd = mkstemp(p2); want(fd >= 0); close(fd); }

    call(build_ingest_repo, repo, kdir);

    SH(GIT_UNSET "cd %s && B=$(git rev-parse HEAD~6) && "
       "git pack-objects --stdout --revs <<EOF > %s\n$B\nEOF",
       repo, p1);
    SH(GIT_UNSET "cd %s && H=$(git rev-parse HEAD) && "
       "git pack-objects --stdout --revs <<EOF > %s\n$H\nEOF",
       repo, p2);

    u8cs root = {(u8cp)kdir, (u8cp)kdir + strlen(kdir)};
    home h = {};
    call(HOMEOpenAt, root, YES);
    call(KEEPOpen, YES);
    call(ingest_pack_file, p1);             // lower history
    call(ingest_pack_file, p2);             // full history (overlaps)
    //  Total stored records must equal the full repo object count —
    //  the overlap from p1 is deduped, not double-stored.
    call(check_ofs_only_and_roundtrip, repo, kdir);
    KEEPClose();
    HOMEClose();

    SH("rm -f %s %s", p1, p2);
    SH("rm -rf %s", repo);
    SH("rm -rf %s", kdir);
    done;
}

ok64 maintest() {
    sane(1);
    call(UNPKtest_single);
    call(UNPKtest_delta_chain);
    call(UNPKtest_mixed);
    call(UNPKtest_ingest_ofs);
    call(UNPKtest_ingest_thin);
    call(UNPKtest_ingest_dedup);
    done;
}

TEST(maintest)
