//  ROUND treadmill: full round-trip test
//
//  1. Create a fresh bare repo with seed content
//  2. Clone via ssh git-upload-pack (our PKT/PACK code) → worktree
//  3. Edit a file, git commit
//  4. Push back via git push over ssh
//  5. Checkout on the origin side
//  6. Verify files are identical
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

// --- helpers ---

static int runcmd(char const *cmd) {
    fprintf(stderr, "  $ %s\n", cmd);
    return system(cmd);
}

static char const *type_str[] = {
    [PACK_OBJ_COMMIT] = "commit",
    [PACK_OBJ_TREE] = "tree",
    [PACK_OBJ_BLOB] = "blob",
    [PACK_OBJ_TAG] = "tag",
};

static ok64 write_loose(char const *gitdir,
                         u8cp objdata, u64 objlen) {
    sane(gitdir && objdata);

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
    snprintf(path, sizeof(path), "%s/objects/%c%c",
             gitdir, hex[0], hex[1]);
    mkdir(path, 0755);

    snprintf(path, sizeof(path), "%s/objects/%c%c/%.38s",
             gitdir, hex[0], hex[1], hex + 2);

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

// Clone a bare repo via SSH into gitdir (must be git init'd).
// Writes loose objects + refs/heads/master + HEAD.
// Returns the HEAD sha hex in out_sha (41 bytes incl NUL).
static ok64 ssh_clone(char const *repo, char const *gitdir,
                       char *out_sha) {
    sane(repo && gitdir && out_sha);

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "ssh -o BatchMode=yes localhost git-upload-pack '%s'", repo);

    int to_child[2], from_child[2];
    if (pipe(to_child) || pipe(from_child)) return GITFAIL;

    pid_t pid = fork();
    if (pid < 0) return GITFAIL;

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

    // read ref advertisement
    u8 rbuf[1 << 20];
    u64 rlen = 0;
    {
        ssize_t n = read(rfd, rbuf, sizeof(rbuf));
        if (n <= 0) return GITFAIL;
        rlen = (u64)n;
    }

    u8cs adv = {rbuf, rbuf + rlen};
    u8cs line = {};
    ok64 o = PKTu8sDrain(adv, line);
    if (o != OK || $len(line) < 40) return GITFAIL;

    memcpy(out_sha, line[0], 40);
    out_sha[40] = 0;

    // drain remaining refs
    for (;;) {
        o = PKTu8sDrain(adv, line);
        if (o == PKTFLUSH) break;
        if (o == NODATA) {
            ssize_t n = read(rfd, rbuf + rlen, sizeof(rbuf) - rlen);
            if (n <= 0) return GITFAIL;
            rlen += (u64)n;
            adv[1] = rbuf + rlen;
            continue;
        }
        if (o != OK) return GITFAIL;
    }

    // send want + done
    {
        u8 wbuf[512];
        u8s ws = {wbuf, wbuf + sizeof(wbuf)};

        u8 pktpay[256];
        int plen = snprintf((char *)pktpay, sizeof(pktpay),
                            "want %.40s no-progress\n", out_sha);
        u8cs payload = {pktpay, pktpay + plen};
        PKTu8sFeed(ws, payload);
        PKTu8sFeedFlush(ws);

        u8 donepay[] = "done\n";
        u8cs donecs = {donepay, donepay + 5};
        PKTu8sFeed(ws, donecs);

        u64 wlen = ws[0] - wbuf;
        write(wfd, wbuf, wlen);
    }
    close(wfd);

    // read response
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
    if (o != OK || $len(line) < 3) return GITFAIL;
    if (memcmp(line[0], "NAK", 3) != 0) return GITFAIL;

    // packfile
    pack_hdr hdr = {};
    o = PACKDrainHdr(resp, &hdr);
    if (o != OK || hdr.version != 2) return GITFAIL;

    fprintf(stderr, "  clone: %u objects\n", hdr.count);

    for (u32 i = 0; i < hdr.count; i++) {
        pack_obj obj = {};
        o = PACKDrainObjHdr(resp, &obj);
        if (o != OK) return o;
        if (obj.type < 1 || obj.type > 4) return PACKBADOBJ;

        u8 content[1 << 18];
        u8s into = {content, content + sizeof(content)};
        o = PACKInflate(resp, into, obj.size);
        if (o != OK) return o;

        u8 objbuf[1 << 18];
        int hdrlen = snprintf((char *)objbuf, sizeof(objbuf),
                              "%s %lu", type_str[obj.type],
                              (unsigned long)obj.size);
        objbuf[hdrlen] = 0;
        memcpy(objbuf + hdrlen + 1, content, obj.size);
        u64 total = hdrlen + 1 + obj.size;

        o = write_loose(gitdir, objbuf, total);
        if (o != OK) return o;
    }

    // write refs
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/refs/heads", gitdir);
        mkdir(path, 0755);

        snprintf(path, sizeof(path), "%s/refs/heads/master", gitdir);
        FILE *f = fopen(path, "w");
        if (!f) return GITFAIL;
        fprintf(f, "%.40s\n", out_sha);
        fclose(f);

        snprintf(path, sizeof(path), "%s/HEAD", gitdir);
        f = fopen(path, "w");
        if (!f) return GITFAIL;
        fprintf(f, "ref: refs/heads/master\n");
        fclose(f);
    }

    { int status; waitpid(pid, &status, 0); }

    done;
}

// --- main test ---

ok64 maintest() {
    sane(1);

    //  Unique per-run scratch root.  $TMPDIR (CMake points it at the
    //  shared test run dir) or /tmp; mkdtemp guarantees uniqueness so
    //  parallel ctest jobs never collide on a shared fixed path.
    char root[256];
    {
        char const *base = getenv("TMPDIR");
        if (!base || !*base) base = "/tmp";
        snprintf(root, sizeof(root), "%s/round-XXXXXX", base);
        want(mkdtemp(root) != NULL);
    }

    char origin[256], worktree[256], checkout[256];
    snprintf(origin, sizeof(origin), "%s/origin", root);
    snprintf(worktree, sizeof(worktree), "%s/copy2", root);
    snprintf(checkout, sizeof(checkout), "%s/checkout", root);

    // --- 1. Create fresh bare origin with seed content ---
    fprintf(stderr, "--- create origin ---\n");
    {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
            "rm -rf %s %s %s && "
            "git init --bare %s 2>/dev/null && "
            "T=$(mktemp -d) && "
            "git init $T 2>/dev/null && "
            "git -C $T config user.email test@test && "
            "git -C $T config user.name Test && "
            "echo 'line one' > $T/file.txt && "
            "echo 'int main(){return 0;}' > $T/prog.c && "
            "git -C $T add . && "
            "git -C $T commit -m 'initial' 2>/dev/null && "
            "git -C $T remote add origin %s && "
            "git -C $T push origin master 2>/dev/null && "
            "rm -rf $T",
            origin, worktree, checkout, origin, origin);
        want(runcmd(cmd) == 0);
    }

    // --- 2. Clone via our SSH+PKT+PACK code ---
    fprintf(stderr, "--- ssh clone ---\n");
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
            "mkdir -p %s/.git && git init --bare %s/.git 2>/dev/null && "
            "git -C %s config core.bare false",
            worktree, worktree, worktree);
        want(runcmd(cmd) == 0);
    }

    char head_sha[41] = {};
    {
        char gitdir[512];
        snprintf(gitdir, sizeof(gitdir), "%s/.git", worktree);
        ok64 o = ssh_clone(origin, gitdir, head_sha);
        want(o == OK);
    }
    fprintf(stderr, "  HEAD=%.40s\n", head_sha);

    // checkout working tree
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "git -C %s checkout master 2>&1", worktree);
        want(runcmd(cmd) == 0);
    }

    // verify clone
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "cat %s/file.txt", worktree);
        FILE *p = popen(cmd, "r");
        char buf[256];
        want(fgets(buf, sizeof(buf), p) != NULL);
        want(strcmp(buf, "line one\n") == 0);
        want(pclose(p) == 0);
        fprintf(stderr, "  clone verified\n");
    }

    // --- 3. Edit file and commit ---
    fprintf(stderr, "--- edit + commit ---\n");
    {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
            "echo 'line two' >> %s/file.txt && "
            "git -C %s config user.email test@test && "
            "git -C %s config user.name Test && "
            "git -C %s add file.txt && "
            "git -C %s commit -m 'add line two' 2>/dev/null",
            worktree, worktree, worktree, worktree, worktree);
        want(runcmd(cmd) == 0);
    }

    // --- 4. Push back via ssh ---
    fprintf(stderr, "--- push ---\n");
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
            "git -C %s remote add origin "
            "ssh://localhost%s 2>/dev/null; "
            "git -C %s push origin master 2>&1",
            worktree, origin, worktree);
        want(runcmd(cmd) == 0);
    }

    // --- 5. Checkout on origin side ---
    fprintf(stderr, "--- checkout origin ---\n");
    {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
            "git clone %s %s 2>/dev/null",
            origin, checkout);
        want(runcmd(cmd) == 0);
    }

    // --- 6. Verify files are identical ---
    fprintf(stderr, "--- verify ---\n");
    {
        char cmd[1024];

        // compare file.txt
        snprintf(cmd, sizeof(cmd),
            "diff %s/file.txt %s/file.txt",
            worktree, checkout);
        want(runcmd(cmd) == 0);
        fprintf(stderr, "  file.txt OK\n");

        // compare prog.c
        snprintf(cmd, sizeof(cmd),
            "diff %s/prog.c %s/prog.c",
            worktree, checkout);
        want(runcmd(cmd) == 0);
        fprintf(stderr, "  prog.c OK\n");

        // compare git log
        //  Stage via temp files — `<()` isn't POSIX; on FreeBSD-style
        //  hosts popen()'s /bin/sh is BSD sh, not bash.
        {
            char t1[] = "/tmp/round.XXXXXX", t2[] = "/tmp/round.XXXXXX";
            int f1 = mkstemp(t1); want(f1 >= 0); close(f1);
            int f2 = mkstemp(t2); want(f2 >= 0); close(f2);
            snprintf(cmd, sizeof(cmd),
                "git -C %s log --oneline > %s", worktree, t1);
            want(runcmd(cmd) == 0);
            snprintf(cmd, sizeof(cmd),
                "git -C %s log --oneline > %s", checkout, t2);
            want(runcmd(cmd) == 0);
            snprintf(cmd, sizeof(cmd), "diff %s %s 2>&1", t1, t2);
            want(runcmd(cmd) == 0);
            unlink(t1); unlink(t2);
        }
        fprintf(stderr, "  history OK\n");
    }

    fprintf(stderr, "--- round trip OK ---\n");

    //  Best-effort scratch cleanup (only reached on success; a failing
    //  want() short-circuits and leaves the dir for inspection).
    {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", root);
        (void)runcmd(cmd);
    }

    done;
}

TEST(maintest)
