//  WIREE2EREFETCH — GET-007 end-to-end repro: re-fetching an unchanged
//  `be://`/`file://` repo must NOT regrow the client shard.
//
//  Stages a real git repo (commit c1, then c2 on top) as the SERVER
//  keeper, clones it into a CLIENT keeper, then RE-FETCHES the unchanged
//  server.  Measures the client `.be` byte size before/after the second
//  fetch.  Before the fix the re-fetch re-ships + re-ingests the whole
//  log and the shard grows; after the fix it ships ~0 and stays flat.
//
//  Drives the real WIREFetch client + WIREServeUpload server path, so it
//  covers the negotiation exactly as `be get` does.  Set WIRE_TRACE=1 to
//  dump the server-side watermark trace.

#include "keeper/KEEP.h"
#include "keeper/REFADV.h"
#include "keeper/REFS.h"
#include "keeper/WIRE.h"

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
#include "dog/HOME.h"

#define GIT_UNSET "unset GIT_DIR GIT_WORK_TREE GIT_COMMON_DIR " \
                  "GIT_INDEX_FILE GIT_OBJECT_DIRECTORY && "

static void tmp_rm(char const *path) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    int _ = system(cmd);
    (void)_;
}

//  Recursive byte size of a directory tree.  Walks the tree in C so the
//  helper does not depend on `du -sb` (a GNU-ism the BSD `du` rejects).
static long long dir_bytes(char const *path) {
    DIR *d = opendir(path);
    if (!d) return -1;
    long long total = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        char const *nm = e->d_name;
        if (nm[0] == '.' && (nm[1] == 0 || (nm[1] == '.' && nm[2] == 0)))
            continue;
        char child[1100];
        snprintf(child, sizeof(child), "%s/%s", path, nm);
        struct stat st;
        if (lstat(child, &st) != 0) continue;
        if (S_ISREG(st.st_mode)) {
            total += (long long)st.st_size;
        } else if (S_ISDIR(st.st_mode)) {
            long long sub = dir_bytes(child);
            if (sub > 0) total += sub;
        }
    }
    closedir(d);
    return total;
}

static ok64 stage_git_commit(char const *gitdir, char const *content,
                             char *out_hex_41, char *pack_path, size_t pcap) {
    sane(gitdir && content && out_hex_41 && pack_path);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             GIT_UNSET
             "cd %s && git init -q && "
             "git config user.email t@t && git config user.name t && "
             "printf '%s' > a.txt && git add a.txt && git commit -q -m c1",
             gitdir, content);
    if (system(cmd) != 0) return FAIL;
    snprintf(cmd, sizeof(cmd), GIT_UNSET "cd %s && git rev-parse HEAD", gitdir);
    FILE *fp = popen(cmd, "r");
    if (!fp) return FAIL;
    if (fread(out_hex_41, 1, 40, fp) != 40) { pclose(fp); return FAIL; }
    out_hex_41[40] = 0;
    pclose(fp);
    snprintf(pack_path, pcap, "%s/objects.pack", gitdir);
    snprintf(cmd, sizeof(cmd),
             GIT_UNSET "cd %s && git rev-list --objects HEAD | "
             "git pack-objects --stdout > %s", gitdir, pack_path);
    if (system(cmd) != 0) return FAIL;
    done;
}

static ok64 stage_git_commit2(char const *gitdir, char const *content,
                              char *out_hex_41, char *pack_path, size_t pcap) {
    sane(gitdir && content && out_hex_41 && pack_path);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             GIT_UNSET "cd %s && printf '%s' > b.txt && "
             "git add b.txt && git commit -q -m c2", gitdir, content);
    if (system(cmd) != 0) return FAIL;
    snprintf(cmd, sizeof(cmd), GIT_UNSET "cd %s && git rev-parse HEAD", gitdir);
    FILE *fp = popen(cmd, "r");
    if (!fp) return FAIL;
    if (fread(out_hex_41, 1, 40, fp) != 40) { pclose(fp); return FAIL; }
    out_hex_41[40] = 0;
    pclose(fp);
    snprintf(pack_path, pcap, "%s/objects2.pack", gitdir);
    snprintf(cmd, sizeof(cmd),
             GIT_UNSET "cd %s && git rev-list --objects HEAD | "
             "git pack-objects --stdout > %s", gitdir, pack_path);
    if (system(cmd) != 0) return FAIL;
    done;
}

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

//  Ingest a pack into the SERVER keeper + seed the trunk ref.
static ok64 server_ingest(char const *root, char const *pack_path,
                          char const *hex_40) {
    sane(root && pack_path && hex_40);
    static u8 pbuf[1 << 20];
    size_t plen = 0;
    call(slurp_file, pack_path, pbuf, sizeof(pbuf), &plen);

    a_cstr(root_s, root);
    home h = {};
    call(HOMEOpenAt, &h, root_s, YES);
    call(KEEPOpen, &h, YES);
    u8csc bytes = {pbuf, pbuf + plen};
    call(KEEPIngestFile, bytes);

    a_path(keepdir);
    call(HOMEBranchDir, KEEP.h, keepdir, NULL);
    a_pad(u8, kbuf, 8);
    u8bFeed1(kbuf, '?');
    a_dup(u8c, key, u8bData(kbuf));
    u8csc val = {(u8cp)hex_40, (u8cp)hex_40 + 40};
    call(REFSAppend, $path(keepdir), key, val);

    call(KEEPClose);
    HOMEClose(&h);
    done;
}

#define FILE_URI(name, path)                                            \
    char _##name##_buf[1024];                                           \
    snprintf(_##name##_buf, sizeof(_##name##_buf), "file://%s", path);  \
    u8csc name = {(u8cp)_##name##_buf,                                  \
                  (u8cp)_##name##_buf + strlen(_##name##_buf)}

//  One WIREFetch of the server trunk into the client keeper at `root`.
static ok64 client_fetch(char const *root, char const *server) {
    sane(root && server);
    a_cstr(root_s, root);
    home h = {};
    call(HOMEOpenAt, &h, root_s, YES);
    call(KEEPOpen, &h, YES);
    FILE_URI(uri, server);
    u8csc want_cs = {NULL, NULL};   // empty want = trunk
    ok64 fo = WIREFetch(uri, want_cs);
    KEEPClose();
    HOMEClose(&h);
    return fo;
}

ok64 WIREE2EREFETCHtest_unchanged() {
    sane(1);
    call(FILEInit);

    char gitdir[]    = "/tmp/e2e-git-XXXXXX";
    char serverdir[] = "/tmp/e2e-srv-XXXXXX";
    char clientdir[] = "/tmp/e2e-cli-XXXXXX";
    want(mkdtemp(gitdir)    != NULL);
    want(mkdtemp(serverdir) != NULL);
    want(mkdtemp(clientdir) != NULL);

    //  Server: c1 then c2 (two commits, shared history) ingested as two
    //  packs into the trunk file_id, tip = c2.
    char hex1[41], hex2[41];
    char pack1[1024], pack2[1024];
    call(stage_git_commit,  gitdir, "alpha\\n", hex1, pack1, sizeof(pack1));
    call(stage_git_commit2, gitdir, "bravo\\n", hex2, pack2, sizeof(pack2));
    call(server_ingest, serverdir, pack1, hex1);
    call(server_ingest, serverdir, pack2, hex2);

    //  Client: first clone (cold) — fetches the whole history.
    call(client_fetch, clientdir, serverdir);

    char be1[1100];
    snprintf(be1, sizeof(be1), "%s/.be", clientdir);
    long long before = dir_bytes(be1);
    want(before > 0);

    //  Re-fetch the UNCHANGED server.  Must ship ~0 and not regrow.
    call(client_fetch, clientdir, serverdir);
    long long after = dir_bytes(be1);
    want(after > 0);

    fprintf(stderr, "[E2E] client .be before=%lld after=%lld delta=%lld\n",
            before, after, after - before);

    //  THE BUG: a re-fetch of an unchanged repo must not regrow the
    //  shard.  Allow a small slack for REFS-row append churn (<4 KiB).
    want(after - before < 4096);

    tmp_rm(gitdir);
    tmp_rm(serverdir);
    tmp_rm(clientdir);
    done;
}

ok64 maintest() {
    sane(1);
    fprintf(stderr, "WIREE2EREFETCHtest_unchanged...\n");
    call(WIREE2EREFETCHtest_unchanged);
    fprintf(stderr, "all passed\n");
    done;
}

TEST(maintest)
