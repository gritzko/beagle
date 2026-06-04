//  WIRE_CLIENT — `WIREFetch` / `WIREPush` end-to-end smoke tests
//  (WIRE.md Phase 7).
//
//  Cases:
//    1. Fetch smoke: stage a repo with a real commit pack on disk,
//       point a fresh local keeper at it via `WIREFetch(file://…,
//       "heads/main")`, verify the pack ingested + REFS updated.
//    2. Push smoke: stage a local keeper holding a commit, push it
//       via `WIREPush(file://…, "heads/feat")` to a fresh receiver,
//       verify the receiver REFS holds the new tip.
//    3. Round trip: A pushes to B via WIREPush, B fetches from A
//       via WIREFetch — both repos agree on the tip.

#include "keeper/KEEP.h"
#include "dog/git/PKT.h"
#include "keeper/REFADV.h"
#include "keeper/REFS.h"
#include "keeper/WIRE.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/PRO.h"
#include "abc/TEST.h"
#include "dog/HOME.h"

#define GIT_UNSET "unset GIT_DIR GIT_WORK_TREE GIT_COMMON_DIR " \
                  "GIT_INDEX_FILE GIT_OBJECT_DIRECTORY && "

static void tmp_rm(char const *path) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    int _ = system(cmd);
    (void)_;
}

//  Build a one-commit git repo at gitdir, writing the commit's 40-hex
//  SHA into out_hex_41 and the on-disk pack path into pack_path.
static ok64 stage_git_commit(char const *gitdir, char const *content,
                             char *out_hex_41, char *pack_path,
                             size_t pcap) {
    sane(gitdir && content && out_hex_41 && pack_path);
    char cmd[2048];
    int rc;

    snprintf(cmd, sizeof(cmd),
             GIT_UNSET
             "cd %s && git init -q && "
             "git config user.email t@t && git config user.name t && "
             "printf '%s' > a.txt && "
             "git add a.txt && git commit -q -m c1",
             gitdir, content);
    rc = system(cmd);
    if (rc != 0) return FAIL;

    snprintf(cmd, sizeof(cmd),
             GIT_UNSET
             "cd %s && git rev-parse HEAD",
             gitdir);
    FILE *fp = popen(cmd, "r");
    if (!fp) return FAIL;
    if (fread(out_hex_41, 1, 40, fp) != 40) { pclose(fp); return FAIL; }
    out_hex_41[40] = 0;
    pclose(fp);

    snprintf(pack_path, pcap, "%s/objects.pack", gitdir);
    snprintf(cmd, sizeof(cmd),
             GIT_UNSET
             "cd %s && git rev-list --objects HEAD | "
             "git pack-objects --stdout > %s",
             gitdir, pack_path);
    rc = system(cmd);
    if (rc != 0) return FAIL;
    done;
}

//  Extend an existing git repo at gitdir with a second commit c2 on
//  top of c1 (shared history).  Writes c2's 40-hex into out_hex_41 and
//  a pack covering the FULL history (c1+c2) into pack_path.
static ok64 stage_git_commit2(char const *gitdir, char const *content,
                              char *out_hex_41, char *pack_path,
                              size_t pcap) {
    sane(gitdir && content && out_hex_41 && pack_path);
    char cmd[2048];
    int rc;

    snprintf(cmd, sizeof(cmd),
             GIT_UNSET
             "cd %s && "
             "printf '%s' > b.txt && "
             "git add b.txt && git commit -q -m c2",
             gitdir, content);
    rc = system(cmd);
    if (rc != 0) return FAIL;

    snprintf(cmd, sizeof(cmd),
             GIT_UNSET
             "cd %s && git rev-parse HEAD",
             gitdir);
    FILE *fp = popen(cmd, "r");
    if (!fp) return FAIL;
    if (fread(out_hex_41, 1, 40, fp) != 40) { pclose(fp); return FAIL; }
    out_hex_41[40] = 0;
    pclose(fp);

    snprintf(pack_path, pcap, "%s/objects2.pack", gitdir);
    snprintf(cmd, sizeof(cmd),
             GIT_UNSET
             "cd %s && git rev-list --objects HEAD | "
             "git pack-objects --stdout > %s",
             gitdir, pack_path);
    rc = system(cmd);
    if (rc != 0) return FAIL;
    done;
}

//  Slurp a file into buf (caller-allocated).  Updates *out_len.
static ok64 slurp_file(char const *path, u8 *buf, size_t cap, size_t *out_len) {
    sane(path && buf && out_len);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return FAIL;
    size_t off = 0;
    for (;;) {
        if (off >= cap) { close(fd); return FAIL; }
        ssize_t n = read(fd, buf + off, cap - off);
        if (n < 0) { close(fd); return FAIL; }
        if (n == 0) break;
        off += (size_t)n;
    }
    close(fd);
    *out_len = off;
    done;
}

//  Ingest `pack_path` into a fresh keeper at `keeper_root`, plus seed
//  a REFS entry for the named be-branch → hex.  `branch` is be-side
//  (use `""` for the trunk shard; on the wire trunk maps to
//  `refs/heads/main`).
static ok64 stage_local_keeper(char const *keeper_root, char const *pack_path,
                               char const *branch, char const *hex_40) {
    sane(keeper_root && pack_path && branch && hex_40);

    static u8 pbuf[1 << 20];
    size_t plen = 0;
    call(slurp_file, pack_path, pbuf, sizeof(pbuf), &plen);

    a_cstr(root_s, keeper_root);
    home h = {};
    call(HOMEOpenAt, &h, root_s, YES);
    call(KEEPOpen, &h, YES);

    u8csc bytes = {pbuf, pbuf + plen};
    call(KEEPIngestFile, bytes);

    a_path(keepdir);
    call(HOMEBranchDir, KEEP.h, keepdir, NULL);
    a_pad(u8, kbuf, 256);
    u8bFeed1(kbuf, '?');
    if (branch && *branch) {
        u8csc br = {(u8cp)branch, (u8cp)branch + strlen(branch)};
        u8bFeed(kbuf, br);
    }
    a_dup(u8c, key, u8bData(kbuf));

    u8csc val = {(u8cp)hex_40, (u8cp)hex_40 + 40};

    call(REFSAppend, $path(keepdir), key, val);

    call(KEEPClose);
    HOMEClose(&h);
    done;
}

//  Ingest `pack_path` into a fresh keeper at `keeper_root` WITHOUT
//  seeding any REFS row — the shard holds the objects but advertises
//  zero refs.  Models a sub-shard whose only registration row never
//  resolved to a tip, or a detached pin not reachable from any
//  advertised ref: `WIREFetchAll` (fetch every advertised ref) cannot
//  carry the pin, so a want-by-hash fetch is the only path.
static ok64 stage_local_keeper_norefs(char const *keeper_root,
                                      char const *pack_path) {
    sane(keeper_root && pack_path);

    static u8 pbuf[1 << 20];
    size_t plen = 0;
    call(slurp_file, pack_path, pbuf, sizeof(pbuf), &plen);

    a_cstr(root_s, keeper_root);
    home h = {};
    call(HOMEOpenAt, &h, root_s, YES);
    call(KEEPOpen, &h, YES);

    u8csc bytes = {pbuf, pbuf + plen};
    call(KEEPIngestFile, bytes);

    call(KEEPClose);
    HOMEClose(&h);
    done;
}

//  Look up the REFS tip for be-branch `branch` (`""` = trunk) in a
//  keeper, copying 40 hex bytes into out_41.  Returns NO if missing.
static b8 lookup_local_ref(char const *keeper_root, char const *branch,
                           char *out_41) {
    a_cstr(root_s, keeper_root);
    home h = {};
    if (HOMEOpenAt(&h, root_s, NO) != OK) return NO;
    if (KEEPOpen(&h, NO) != OK) { HOMEClose(&h); return NO; }
    a_path(keepdir);
    (void)HOMEBranchDir(KEEP.h, keepdir, NULL);

    a_pad(u8, kbuf, 256);
    u8bFeed1(kbuf, '?');
    if (branch && *branch) {
        u8csc br = {(u8cp)branch, (u8cp)branch + strlen(branch)};
        u8bFeed(kbuf, br);
    }
    a_dup(u8c, key, u8bData(kbuf));

    a_pad(u8, arena, 256);
    uri res = {};
    ok64 ro = REFSResolve(&res, arena, $path(keepdir), key);
    b8 found = NO;
    if (ro == OK && u8csLen(res.query) == 40) {
        memcpy(out_41, res.query[0], 40);
        out_41[40] = 0;
        found = YES;
    }
    KEEPClose();
    HOMEClose(&h);
    return found;
}

//  Build a "file:///<path>" URI slice on the stack.
#define FILE_URI(name, path)                                            \
    char _##name##_buf[1024];                                           \
    snprintf(_##name##_buf, sizeof(_##name##_buf), "file://%s", path);  \
    u8csc name = {(u8cp)_##name##_buf,                                  \
                  (u8cp)_##name##_buf + strlen(_##name##_buf)}

// ---- Test 1: fetch smoke -----------------------------------------------

ok64 WIRECLIENTtest_fetch_smoke() {
    sane(1);
    call(FILEInit);

    char gitdir[]    = "/tmp/wcli-fetch-git-XXXXXX";
    want(mkdtemp(gitdir) != NULL);
    char serverdir[] = "/tmp/wcli-fetch-srv-XXXXXX";
    want(mkdtemp(serverdir) != NULL);
    char clientdir[] = "/tmp/wcli-fetch-cli-XXXXXX";
    want(mkdtemp(clientdir) != NULL);

    //  Build a real one-commit pack and ingest it into the "server"
    //  keeper, then put it under the trunk shard.  Trunk advertises
    //  on the wire as `refs/heads/main` (the only wire alias).
    char hex[41];
    char packpath[1024];
    call(stage_git_commit, gitdir, "alpha\\n", hex, packpath, sizeof(packpath));
    call(stage_local_keeper, serverdir, packpath, "", hex);

    //  Fetch from server into a fresh client keeper.  Empty want
    //  selects the trunk (peer's HEAD-mapped branch).
    {
        a_cstr(client_root_s, clientdir);
        home h = {};
        call(HOMEOpenAt, &h, client_root_s, YES);
        call(KEEPOpen, &h, YES);

        FILE_URI(uri, serverdir);
        u8csc want_cs = {NULL, NULL};
        ok64 fo = WIREFetch(uri, want_cs);
        want(fo == OK);

        KEEPClose();
        HOMEClose(&h);
    }

    //  Verify the client REFS now holds the same tip on its trunk.
    char got[41];
    want(lookup_local_ref(clientdir, "", got));
    want(memcmp(got, hex, 40) == 0);

    tmp_rm(gitdir);
    tmp_rm(serverdir);
    tmp_rm(clientdir);
    done;
}

// ---- Test 2: push smoke ------------------------------------------------

ok64 WIRECLIENTtest_push_smoke() {
    sane(1);
    call(FILEInit);

    char gitdir[]    = "/tmp/wcli-push-git-XXXXXX";
    want(mkdtemp(gitdir) != NULL);
    char srcdir[]    = "/tmp/wcli-push-src-XXXXXX";
    want(mkdtemp(srcdir) != NULL);
    char dstdir[]    = "/tmp/wcli-push-dst-XXXXXX";
    want(mkdtemp(dstdir) != NULL);

    //  Source keeper: a fresh commit ingested under be-branch "feat".
    char hex[41];
    char packpath[1024];
    call(stage_git_commit, gitdir, "alpha\\n", hex, packpath, sizeof(packpath));
    call(stage_local_keeper, srcdir, packpath, "feat", hex);

    //  Push from source to destination.  local_branch is be-side.
    {
        a_cstr(src_root_s, srcdir);
        home h = {};
        call(HOMEOpenAt, &h, src_root_s, YES);
        call(KEEPOpen, &h, YES);

        FILE_URI(uri, dstdir);
        a_cstr(branch_s, "feat");
        u8csc branch_cs = {branch_s[0], branch_s[1]};
        sha1 tip = {};
        u8s bin = {tip.data, tip.data + 20};
        u8cs hx = {(u8cp)hex, (u8cp)hex + 40};
        want(HEXu8sDrainSome(bin, hx) == OK);
        ok64 po = WIREPush(uri, branch_cs, &tip, NO);
        want(po == OK);

        KEEPClose();
        HOMEClose(&h);
    }

    //  Destination should now have be-branch "feat" → hex.
    char got[41];
    want(lookup_local_ref(dstdir, "feat", got));
    want(memcmp(got, hex, 40) == 0);

    tmp_rm(gitdir);
    tmp_rm(srcdir);
    tmp_rm(dstdir);
    done;
}

// ---- Test 3: round trip -----------------------------------------------

ok64 WIRECLIENTtest_round_trip() {
    sane(1);
    call(FILEInit);

    char gitdir[]    = "/tmp/wcli-rt-git-XXXXXX";
    want(mkdtemp(gitdir) != NULL);
    char Adir[]      = "/tmp/wcli-rt-A-XXXXXX";
    want(mkdtemp(Adir) != NULL);
    char Bdir[]      = "/tmp/wcli-rt-B-XXXXXX";
    want(mkdtemp(Bdir) != NULL);
    char Cdir[]      = "/tmp/wcli-rt-C-XXXXXX";
    want(mkdtemp(Cdir) != NULL);

    char hex[41];
    char packpath[1024];
    call(stage_git_commit, gitdir, "round-trip\\n", hex, packpath,
         sizeof(packpath));

    //  A holds the commit on its trunk (wire-side `refs/heads/main`).
    call(stage_local_keeper, Adir, packpath, "", hex);

    //  Push A → B (trunk → wire main).
    {
        a_cstr(A_root_s, Adir);
        home h = {};
        call(HOMEOpenAt, &h, A_root_s, YES);
        call(KEEPOpen, &h, YES);

        FILE_URI(uri, Bdir);
        u8csc branch_cs = {NULL, NULL};
        sha1 tip = {};
        u8s bin = {tip.data, tip.data + 20};
        u8cs hx = {(u8cp)hex, (u8cp)hex + 40};
        want(HEXu8sDrainSome(bin, hx) == OK);
        ok64 po = WIREPush(uri, branch_cs, &tip, NO);
        want(po == OK);

        KEEPClose();
        HOMEClose(&h);
    }

    //  Fetch A → C (through fresh keeper C).
    {
        a_cstr(C_root_s, Cdir);
        home h = {};
        call(HOMEOpenAt, &h, C_root_s, YES);
        call(KEEPOpen, &h, YES);

        FILE_URI(uri, Adir);
        u8csc want_cs = {NULL, NULL};
        ok64 fo = WIREFetch(uri, want_cs);
        want(fo == OK);

        KEEPClose();
        HOMEClose(&h);
    }

    //  Both B and C agree with A on the trunk tip.
    char gotB[41], gotC[41];
    want(lookup_local_ref(Bdir, "", gotB));
    want(memcmp(gotB, hex, 40) == 0);
    want(lookup_local_ref(Cdir, "", gotC));
    want(memcmp(gotC, hex, 40) == 0);

    tmp_rm(gitdir);
    tmp_rm(Adir);
    tmp_rm(Bdir);
    tmp_rm(Cdir);
    done;
}

// ---- Test 4: fetch by pin (want-by-hash) ------------------------------
//
//  The submodule fix: the parent holds the gitlink pin sha and must
//  fetch THAT exact commit, regardless of what the source shard
//  advertises.  Here the server shard holds the commit but advertises
//  ZERO refs.  `WIREFetch(uri, <40-hex pin>)` must send `want <pin>`
//  directly (bypassing advertisement matching) and land the object.
//
//  Pre-fix this fails: wcli_match_advert treats the 40-hex want as a
//  branch name, finds no matching advertised ref, and returns
//  WIRECLNRF.  Post-fix, a 40-hex want_ref decodes straight to the
//  want sha; the server's wire_locate_sha serves any present object.
ok64 WIRECLIENTtest_fetch_by_pin() {
    sane(1);
    call(FILEInit);

    char gitdir[]    = "/tmp/wcli-pin-git-XXXXXX";
    want(mkdtemp(gitdir) != NULL);
    char serverdir[] = "/tmp/wcli-pin-srv-XXXXXX";
    want(mkdtemp(serverdir) != NULL);
    char clientdir[] = "/tmp/wcli-pin-cli-XXXXXX";
    want(mkdtemp(clientdir) != NULL);

    //  Server holds the pin commit's objects but NO advertised ref.
    char hex[41];
    char packpath[1024];
    call(stage_git_commit, gitdir, "pinned\\n", hex, packpath,
         sizeof(packpath));
    call(stage_local_keeper_norefs, serverdir, packpath);

    //  Fetch the pin by hash into a fresh client keeper.
    {
        a_cstr(client_root_s, clientdir);
        home h = {};
        call(HOMEOpenAt, &h, client_root_s, YES);
        call(KEEPOpen, &h, YES);

        FILE_URI(uri, serverdir);
        u8csc pin_cs = {(u8cp)hex, (u8cp)hex + 40};
        ok64 fo = WIREFetch(uri, pin_cs);
        want(fo == OK);

        //  The pinned object must now be present (+ integrity-verified)
        //  in the client keeper — proves the want-by-hash pull worked
        //  even though the server advertised nothing.
        a_dup(u8c, pin_hx, pin_cs);
        want(KEEPVerify(pin_hx) == OK);

        KEEPClose();
        HOMEClose(&h);
    }

    //  Piece B tie-in: the by-pin fetch records the pin as the shard's
    //  trunk (the clone `get`-row counts as the trunk ref), so a later
    //  serve of this client re-advertises it.
    char got[41];
    want(lookup_local_ref(clientdir, "", got));
    want(memcmp(got, hex, 40) == 0);

    tmp_rm(gitdir);
    tmp_rm(serverdir);
    tmp_rm(clientdir);
    done;
}

// ---- Test 5: title clash — disjoint history is refused ----------------
//
//  DIS-012: a shard already holds project `t` (history A).  A `be get`
//  of an UNRELATED repo (history B, no common ancestor) forcing the
//  same title must REFUSE with TITLECLSH — never co-mingle B's commit
//  into A's referenced history.  Pre-fix this case mis-passes: the
//  unrelated tip is silently recorded as the trunk.
ok64 WIRECLIENTtest_title_clash() {
    sane(1);
    call(FILEInit);

    char gitA[]      = "/tmp/wcli-clash-A-XXXXXX";
    want(mkdtemp(gitA) != NULL);
    char gitB[]      = "/tmp/wcli-clash-B-XXXXXX";
    want(mkdtemp(gitB) != NULL);
    char serverB[]   = "/tmp/wcli-clash-srvB-XXXXXX";
    want(mkdtemp(serverB) != NULL);
    char clientdir[] = "/tmp/wcli-clash-cli-XXXXXX";
    want(mkdtemp(clientdir) != NULL);

    //  Repo A → client shard trunk (the established project `t`).
    char hexA[41], packA[1024];
    call(stage_git_commit, gitA, "alpha\\n", hexA, packA, sizeof(packA));
    call(stage_local_keeper, clientdir, packA, "", hexA);

    //  Unrelated repo B → its own server shard, trunk.
    char hexB[41], packB[1024];
    call(stage_git_commit, gitB, "bravo\\n", hexB, packB, sizeof(packB));
    call(stage_local_keeper, serverB, packB, "", hexB);

    //  Fetch B into the client shard that already holds A.  Disjoint
    //  histories under one title ⇒ TITLECLSH.
    {
        a_cstr(client_root_s, clientdir);
        home h = {};
        call(HOMEOpenAt, &h, client_root_s, YES);
        call(KEEPOpen, &h, YES);

        FILE_URI(uri, serverB);
        u8csc want_cs = {NULL, NULL};
        ok64 fo = WIREFetch(uri, want_cs);
        want(fo == TITLECLSH);

        KEEPClose();
        HOMEClose(&h);
    }

    //  The client trunk must still point at A — B never co-mingled.
    char got[41];
    want(lookup_local_ref(clientdir, "", got));
    want(memcmp(got, hexA, 40) == 0);

    tmp_rm(gitA);
    tmp_rm(gitB);
    tmp_rm(serverB);
    tmp_rm(clientdir);
    done;
}

// ---- Test 6: same title, shared history still converges ---------------
//
//  DIS-012 negative case: the SAME title with a COMMON ancestor is the
//  normal mirror/converge path — one shard.  A shard holding A(c1)
//  fetching A's descendant (c1→c2) must advance, not refuse.
ok64 WIRECLIENTtest_title_converge() {
    sane(1);
    call(FILEInit);

    char gitdir[]    = "/tmp/wcli-conv-git-XXXXXX";
    want(mkdtemp(gitdir) != NULL);
    char serverdir[] = "/tmp/wcli-conv-srv-XXXXXX";
    want(mkdtemp(serverdir) != NULL);
    char clientdir[] = "/tmp/wcli-conv-cli-XXXXXX";
    want(mkdtemp(clientdir) != NULL);

    //  c1 → client shard trunk.
    char hex1[41], pack1[1024];
    call(stage_git_commit, gitdir, "alpha\\n", hex1, pack1, sizeof(pack1));
    call(stage_local_keeper, clientdir, pack1, "", hex1);

    //  c2 (descendant of c1) → server shard trunk; full history pack.
    char hex2[41], pack2[1024];
    call(stage_git_commit2, gitdir, "beta\\n", hex2, pack2, sizeof(pack2));
    call(stage_local_keeper, serverdir, pack2, "", hex2);

    //  Fetch the descendant into the shard holding the ancestor.
    //  Shared history ⇒ converge (OK), trunk advances to c2.
    {
        a_cstr(client_root_s, clientdir);
        home h = {};
        call(HOMEOpenAt, &h, client_root_s, YES);
        call(KEEPOpen, &h, YES);

        FILE_URI(uri, serverdir);
        u8csc want_cs = {NULL, NULL};
        ok64 fo = WIREFetch(uri, want_cs);
        want(fo == OK);

        KEEPClose();
        HOMEClose(&h);
    }

    char got[41];
    want(lookup_local_ref(clientdir, "", got));
    want(memcmp(got, hex2, 40) == 0);

    tmp_rm(gitdir);
    tmp_rm(serverdir);
    tmp_rm(clientdir);
    done;
}

//  Decode a 40-hex sha (NUL-terminated `hex`) into a sha1 on the stack.
#define HEX2SHA(name, hex)                                       \
    sha1 name = {};                                              \
    do {                                                         \
        u8s _b = {name.data, name.data + 20};                    \
        u8cs _h = {(u8cp)(hex), (u8cp)(hex) + 40};               \
        want(HEXu8sDrainSome(_b, _h) == OK);                     \
    } while (0)

// ---- Test 7: incremental push prunes the have-set (DIS-021) -----------
//
//  B already holds c1 on its trunk.  A holds c1+c2 (c2 is c1's child).
//  Pushing c2 → B must send ONLY c2's new objects (its commit, its
//  tree, the new blob b.txt = 3), NOT the full c1+c2 closure (≥5).
//  The have-set, seeded from B's advertised c1 tip, prunes the shared
//  history.  `WIREPushLastObjCount` is the observable.
ok64 WIRECLIENTtest_incremental_prune() {
    sane(1);
    call(FILEInit);

    char gitdir[]    = "/tmp/wcli-incr-git-XXXXXX";
    want(mkdtemp(gitdir) != NULL);
    char Adir[]      = "/tmp/wcli-incr-A-XXXXXX";
    want(mkdtemp(Adir) != NULL);
    char Bdir[]      = "/tmp/wcli-incr-B-XXXXXX";
    want(mkdtemp(Bdir) != NULL);

    //  c1, then c2 (c1's child); full-history pack covers both.
    char hex1[41], pack1[1024];
    call(stage_git_commit, gitdir, "alpha\\n", hex1, pack1, sizeof(pack1));
    char hex2[41], pack2[1024];
    call(stage_git_commit2, gitdir, "beta\\n", hex2, pack2, sizeof(pack2));

    //  A holds the full c1+c2 history on its trunk.
    call(stage_local_keeper, Adir, pack2, "", hex2);
    //  B holds ONLY c1 on its trunk (its single advertised tip).
    call(stage_local_keeper, Bdir, pack1, "", hex1);

    //  Push c2 from A → B.  B advertises c1, so the have-set prunes
    //  c1's commit/tree/blob; only c2's new objects ship.
    u32 nshas = 0;
    {
        a_cstr(A_root_s, Adir);
        home h = {};
        call(HOMEOpenAt, &h, A_root_s, YES);
        call(KEEPOpen, &h, YES);

        FILE_URI(uri, Bdir);
        u8csc branch_cs = {NULL, NULL};
        HEX2SHA(tip2, hex2);
        ok64 po = WIREPush(uri, branch_cs, &tip2, NO);
        want(po == OK);
        nshas = WIREPushLastObjCount;

        KEEPClose();
        HOMEClose(&h);
    }

    //  B's trunk advanced to c2.
    char got[41];
    want(lookup_local_ref(Bdir, "", got));
    want(memcmp(got, hex2, 40) == 0);

    //  The pruned pack carries only c2's introduced objects (commit +
    //  its tree + the new blob = 3), NOT the full c1+c2 closure (≥5).
    //  A loose `< 5` makes a pruning regression (full closure resent)
    //  brutally obvious without over-pinning the exact object count.
    fprintf(stderr, "incremental_prune: pushed %u objects\n", nshas);
    want(nshas > 0);
    want(nshas < 5);

    tmp_rm(gitdir);
    tmp_rm(Adir);
    tmp_rm(Bdir);
    done;
}

// ---- Test 8: up-to-date push builds no pack (DIS-021) -----------------
//
//  B already holds c1 on its trunk.  Pushing c1 again must short-circuit
//  (peer already at tip): OK, no pack built (`WIREPushLastObjCount == 0`).
ok64 WIRECLIENTtest_uptodate_nopack() {
    sane(1);
    call(FILEInit);

    char gitdir[]    = "/tmp/wcli-utd-git-XXXXXX";
    want(mkdtemp(gitdir) != NULL);
    char Adir[]      = "/tmp/wcli-utd-A-XXXXXX";
    want(mkdtemp(Adir) != NULL);
    char Bdir[]      = "/tmp/wcli-utd-B-XXXXXX";
    want(mkdtemp(Bdir) != NULL);

    char hex1[41], pack1[1024];
    call(stage_git_commit, gitdir, "alpha\\n", hex1, pack1, sizeof(pack1));
    call(stage_local_keeper, Adir, pack1, "", hex1);
    call(stage_local_keeper, Bdir, pack1, "", hex1);

    u32 nshas = 99;
    {
        a_cstr(A_root_s, Adir);
        home h = {};
        call(HOMEOpenAt, &h, A_root_s, YES);
        call(KEEPOpen, &h, YES);

        FILE_URI(uri, Bdir);
        u8csc branch_cs = {NULL, NULL};
        HEX2SHA(tip1, hex1);
        ok64 po = WIREPush(uri, branch_cs, &tip1, NO);
        want(po == OK);
        nshas = WIREPushLastObjCount;

        KEEPClose();
        HOMEClose(&h);
    }

    //  Peer already at tip ⇒ no pack built.
    want(nshas == 0);

    tmp_rm(gitdir);
    tmp_rm(Adir);
    tmp_rm(Bdir);
    done;
}

// ---- Test 9: non-FF push builds no pack (DIS-021) ---------------------
//
//  B holds c2 (ahead).  A pushes c1 (an ancestor of c2, so a non-FF
//  rewind) without force.  The client FF gate must refuse with
//  WIRECLNFF BEFORE building the pack (`WIREPushLastObjCount == 0`), and
//  B's trunk must stay at c2.
ok64 WIRECLIENTtest_nonff_nopack() {
    sane(1);
    call(FILEInit);

    char gitdir[]    = "/tmp/wcli-nff-git-XXXXXX";
    want(mkdtemp(gitdir) != NULL);
    char Adir[]      = "/tmp/wcli-nff-A-XXXXXX";
    want(mkdtemp(Adir) != NULL);
    char Bdir[]      = "/tmp/wcli-nff-B-XXXXXX";
    want(mkdtemp(Bdir) != NULL);

    char hex1[41], pack1[1024];
    call(stage_git_commit, gitdir, "alpha\\n", hex1, pack1, sizeof(pack1));
    char hex2[41], pack2[1024];
    call(stage_git_commit2, gitdir, "beta\\n", hex2, pack2, sizeof(pack2));

    //  A holds c1 (the ancestor it will try to push).  B is ahead at c2.
    call(stage_local_keeper, Adir, pack2, "", hex1);
    call(stage_local_keeper, Bdir, pack2, "", hex2);

    u32 nshas = 99;
    ok64 po = WIRENOWANT;
    {
        a_cstr(A_root_s, Adir);
        home h = {};
        call(HOMEOpenAt, &h, A_root_s, YES);
        call(KEEPOpen, &h, YES);

        FILE_URI(uri, Bdir);
        u8csc branch_cs = {NULL, NULL};
        HEX2SHA(tip1, hex1);
        po = WIREPush(uri, branch_cs, &tip1, NO);
        nshas = WIREPushLastObjCount;

        KEEPClose();
        HOMEClose(&h);
    }

    //  Rejected up front: non-FF, no pack built.
    want(po == WIRECLNFF);
    want(nshas == 0);

    //  B unchanged at c2.
    char got[41];
    want(lookup_local_ref(Bdir, "", got));
    want(memcmp(got, hex2, 40) == 0);

    tmp_rm(gitdir);
    tmp_rm(Adir);
    tmp_rm(Bdir);
    done;
}

ok64 maintest() {
    sane(1);
    fprintf(stderr, "WIRECLIENTtest_fetch_smoke...\n");
    call(WIRECLIENTtest_fetch_smoke);
    fprintf(stderr, "WIRECLIENTtest_push_smoke...\n");
    call(WIRECLIENTtest_push_smoke);
    fprintf(stderr, "WIRECLIENTtest_round_trip...\n");
    call(WIRECLIENTtest_round_trip);
    fprintf(stderr, "WIRECLIENTtest_fetch_by_pin...\n");
    call(WIRECLIENTtest_fetch_by_pin);
    fprintf(stderr, "WIRECLIENTtest_title_clash...\n");
    call(WIRECLIENTtest_title_clash);
    fprintf(stderr, "WIRECLIENTtest_title_converge...\n");
    call(WIRECLIENTtest_title_converge);
    fprintf(stderr, "WIRECLIENTtest_incremental_prune...\n");
    call(WIRECLIENTtest_incremental_prune);
    fprintf(stderr, "WIRECLIENTtest_uptodate_nopack...\n");
    call(WIRECLIENTtest_uptodate_nopack);
    fprintf(stderr, "WIRECLIENTtest_nonff_nopack...\n");
    call(WIRECLIENTtest_nonff_nopack);
    fprintf(stderr, "all passed\n");
    done;
}

TEST(maintest)
