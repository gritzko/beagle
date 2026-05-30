//  FETCH treadmill: clone a repo via ssh git-upload-pack,
//  unpack objects, write them as loose objects into a new repo.
//
#include "dog/git/GIT.h"
#include "dog/git/PACK.h"
#include "dog/git/PKT.h"
#include "dog/git/SHA1.h"
#include "dog/git/ZINF.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "abc/HEX.h"
#include "abc/PRO.h"
#include "abc/TEST.h"

// Write a loose git object into <outdir>/objects/<xx>/<38hex>
static ok64 write_loose(u8cp outdir, u64 outdirlen,
                         u8cp objdata, u64 objlen) {
    sane(outdir && objdata);

    u8 sha[20];
    {
        sha1 s = {};
        u8csc src = {objdata, objdata + objlen};
        SHA1Sum(&s, src);
        memcpy(sha, s.data, 20);
    }

    u8 hex[40];
    u8s hexs = {hex, hex + 40};
    u8cs bins = {sha, sha + 20};
    HEXu8sFeedSome(hexs, bins);

    char path[512];
    snprintf(path, sizeof(path), "%.*s/objects/%c%c",
             (int)outdirlen, outdir, hex[0], hex[1]);
    mkdir(path, 0755);

    snprintf(path, sizeof(path), "%.*s/objects/%c%c/%.38s",
             (int)outdirlen, outdir, hex[0], hex[1], hex + 2);

    a_pad(u8, zbuf, 1 << 18);
    u8csc zsrc = {objdata, objdata + objlen};
    u64 idle_before = u8bIdleLen(zbuf);
    ok64 zr = ZINFDeflate(u8bIdle(zbuf), zsrc);
    if (zr != OK) return PACKFAIL;
    u64 zlen = idle_before - u8bIdleLen(zbuf);
    u8bFed(zbuf, zlen);

    FILE *f = fopen(path, "wb");
    if (!f) return GITFAIL;
    fwrite(u8bDataHead(zbuf), 1, zlen, f);
    fclose(f);

    done;
}

static char const *type_str[] = {
    [PACK_OBJ_COMMIT] = "commit",
    [PACK_OBJ_TREE] = "tree",
    [PACK_OBJ_BLOB] = "blob",
    [PACK_OBJ_TAG] = "tag",
};

ok64 maintest() {
    sane(1);

    char const *repo = getenv("FETCH_REPO");
    char const *outdir = getenv("FETCH_OUT");

    //  Unless the caller fully specifies both paths, pick a unique
    //  per-run scratch root.  $TMPDIR (CMake points it at the shared
    //  test run dir) or /tmp; mkdtemp guarantees uniqueness so parallel
    //  ctest jobs never collide on a shared fixed path.
    char root[200] = {};
    char repo_buf[256], out_buf[256];
    if (!repo || !outdir) {
        char const *base = getenv("TMPDIR");
        if (!base || !*base) base = "/tmp";
        snprintf(root, sizeof(root), "%s/fetch-XXXXXX", base);
        want(mkdtemp(root) != NULL);
    }

    //  No source repo supplied: build a throwaway bare fixture seeded
    //  with one README.md commit (mirrors ROUND's origin setup).  The
    //  bare form is required — `repo` is later used as a git --git-dir
    //  and served via git-upload-pack.
    if (!repo) {
        snprintf(repo_buf, sizeof(repo_buf), "%s/repo", root);
        repo = repo_buf;
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
            "git init --bare -b master %s 2>/dev/null && "
            "T=$(mktemp -d) && "
            "git init -b master $T 2>/dev/null && "
            "git -C $T config user.email test@test && "
            "git -C $T config user.name Test && "
            "printf 'hello fetch\\n' > $T/README.md && "
            "git -C $T add . && git -C $T commit -m seed 2>/dev/null && "
            "git -C $T remote add origin %s && "
            "git -C $T push origin master 2>/dev/null && "
            "rm -rf $T",
            repo, repo);
        want(system(cmd) == 0);
    }

    if (!outdir) {
        snprintf(out_buf, sizeof(out_buf), "%s/copy", root);
        outdir = out_buf;
    }

    // --- init output as bare repo ---
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "rm -rf %s && git init --bare %s 2>/dev/null", outdir, outdir);
        want(system(cmd) == 0);
    }

    // --- ssh pipe to git-upload-pack ---
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "ssh -o BatchMode=yes localhost git-upload-pack '%s'", repo);

    int to_child[2], from_child[2];
    want(pipe(to_child) == 0);
    want(pipe(from_child) == 0);

    pid_t pid = fork();
    want(pid >= 0);

    if (pid == 0) {
        close(to_child[1]);
        close(from_child[0]);
        dup2(to_child[0], 0);
        dup2(from_child[1], 1);
        close(to_child[0]);
        close(from_child[1]);
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        _exit(127);
    }

    close(to_child[0]);
    close(from_child[1]);
    int wfd = to_child[1];
    int rfd = from_child[0];

    // --- read ref advertisement ---
    u8 rbuf[1 << 20];
    u64 rlen = 0;
    {
        ssize_t n = read(rfd, rbuf, sizeof(rbuf));
        want(n > 0);
        rlen = (u64)n;
    }

    u8cs adv = {rbuf, rbuf + rlen};
    u8cs line = {};
    ok64 o = PKTu8sDrain(adv, line);
    want(o == OK);
    want($len(line) >= 40);

    u8 want_sha_hex[41];
    memcpy(want_sha_hex, line[0], 40);
    want_sha_hex[40] = 0;

    // drain remaining refs until flush
    for (;;) {
        o = PKTu8sDrain(adv, line);
        if (o == PKTFLUSH) break;
        if (o == NODATA) {
            ssize_t n = read(rfd, rbuf + rlen, sizeof(rbuf) - rlen);
            if (n <= 0) { want(0); }
            rlen += (u64)n;
            adv[1] = rbuf + rlen;
            continue;
        }
        want(o == OK);
    }

    fprintf(stderr, "  HEAD=%.*s\n", 40, want_sha_hex);

    // --- send want + done ---
    {
        u8 wbuf[512];
        u8s ws = {wbuf, wbuf + sizeof(wbuf)};

        u8 pktpay[256];
        int plen = snprintf((char *)pktpay, sizeof(pktpay),
                            "want %.40s no-progress\n", want_sha_hex);
        u8cs payload = {pktpay, pktpay + plen};
        o = PKTu8sFeed(ws, payload);
        want(o == OK);

        o = PKTu8sFeedFlush(ws);
        want(o == OK);

        u8 donepay[] = "done\n";
        u8cs donecs = {donepay, donepay + 5};
        o = PKTu8sFeed(ws, donecs);
        want(o == OK);

        u64 wlen = ws[0] - wbuf;
        want(write(wfd, wbuf, wlen) == (ssize_t)wlen);
    }
    close(wfd);

    // --- read packfile response ---
    rlen = 0;
    for (;;) {
        ssize_t n = read(rfd, rbuf + rlen, sizeof(rbuf) - rlen);
        if (n <= 0) break;
        rlen += (u64)n;
    }
    close(rfd);

    u8cs resp = {rbuf, rbuf + rlen};

    // NAK
    o = PKTu8sDrain(resp, line);
    want(o == OK && $len(line) >= 3);
    want(memcmp(line[0], "NAK", 3) == 0);

    // packfile header
    pack_hdr hdr = {};
    o = PACKDrainHdr(resp, &hdr);
    want(o == OK && hdr.version == 2);

    fprintf(stderr, "  pack: %u objects, %lu bytes\n",
            hdr.count, (unsigned long)rlen);

    // unpack each object
    for (u32 i = 0; i < hdr.count; i++) {
        pack_obj obj = {};
        o = PACKDrainObjHdr(resp, &obj);
        want(o == OK);
        want(obj.type >= 1 && obj.type <= 4);

        u8 content[1 << 18];
        u8s into = {content, content + sizeof(content)};
        o = PACKInflate(resp, into, obj.size);
        want(o == OK);

        // build git object: "<type> <size>\0<content>"
        u8 objbuf[1 << 18];
        int hdrlen = snprintf((char *)objbuf, sizeof(objbuf),
                              "%s %lu", type_str[obj.type],
                              (unsigned long)obj.size);
        objbuf[hdrlen] = 0;
        memcpy(objbuf + hdrlen + 1, content, obj.size);
        u64 total = hdrlen + 1 + obj.size;

        o = write_loose((u8cp)outdir, strlen(outdir), objbuf, total);
        want(o == OK);

        u8 sha[20];
        {
            sha1 s = {};
            u8csc src = {objbuf, objbuf + total};
            SHA1Sum(&s, src);
            memcpy(sha, s.data, 20);
        }
        u8 shex[40];
        u8s ss = {shex, shex + 40};
        u8cs sb = {sha, sha + 20};
        HEXu8sFeedSome(ss, sb);

        fprintf(stderr, "  %s %4lu  %.40s\n",
                type_str[obj.type], (unsigned long)obj.size, shex);
    }

    // --- write refs ---
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/refs/heads", outdir);
        mkdir(path, 0755);

        snprintf(path, sizeof(path), "%s/refs/heads/master", outdir);
        FILE *f = fopen(path, "w");
        want(f != NULL);
        fprintf(f, "%.40s\n", want_sha_hex);
        fclose(f);

        snprintf(path, sizeof(path), "%s/HEAD", outdir);
        f = fopen(path, "w");
        want(f != NULL);
        fprintf(f, "ref: refs/heads/master\n");
        fclose(f);
    }

    // --- verify: git log ---
    {
        char vcmd[512];
        snprintf(vcmd, sizeof(vcmd),
                 "git --git-dir=%s log --oneline 2>&1", outdir);
        FILE *p = popen(vcmd, "r");
        want(p != NULL);
        char buf[1024];
        fprintf(stderr, "  --- git log ---\n");
        while (fgets(buf, sizeof(buf), p))
            fprintf(stderr, "  %s", buf);
        want(pclose(p) == 0);
    }

    // --- verify: content match ---
    //  Stage each side into a temp file then diff; POSIX sh can't do
    //  `<()` process substitution and bash isn't visible to popen()'s
    //  shell on every platform (FreeBSD's /bin/sh doesn't fall back).
    {
        char t1[] = "/tmp/fetch.XXXXXX", t2[] = "/tmp/fetch.XXXXXX";
        int f1 = mkstemp(t1); want(f1 >= 0); close(f1);
        int f2 = mkstemp(t2); want(f2 >= 0); close(f2);
        char vcmd[1024];
        snprintf(vcmd, sizeof(vcmd),
                 "git --git-dir=%s show HEAD:README.md > %s", repo, t1);
        want(system(vcmd) == 0);
        snprintf(vcmd, sizeof(vcmd),
                 "git --git-dir=%s show HEAD:README.md > %s", outdir, t2);
        want(system(vcmd) == 0);
        snprintf(vcmd, sizeof(vcmd), "diff %s %s 2>&1", t1, t2);
        FILE *p = popen(vcmd, "r");
        char buf[1024];
        while (fgets(buf, sizeof(buf), p))
            fprintf(stderr, "  diff: %s", buf);
        want(pclose(p) == 0);
        unlink(t1); unlink(t2);
        fprintf(stderr, "  content OK\n");
    }

    { int status; waitpid(pid, &status, 0); }

    //  Best-effort scratch cleanup when we created the fixture (only
    //  reached on success; a failing want() leaves it for inspection).
    if (root[0]) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", root);
        (void)system(cmd);
    }

    done;
}

TEST(maintest)
