//  Beagle-side submodule recursion plumbing.  See SUBS.h.

#include "SUBS.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"

#include "dog/CLI.h"        // CLI_MAX_FLAGS / CLI_MAX_URIS
#include "dog/git/SUBS.h"
#include "sniff/SUBS.h"     // SNIFFSubIsMount

//  BEDOGEXIT / BEDOGSIG mirror the constants defined in BE.cli.c
//  (which uses `con` = `static const`, so they don't cross
//  translation units).  Values MUST stay in sync — if you change
//  one side, change the other.  RON60-encoded, see abc/ok64.
con ok64 BEDOGEXIT = 0xb38d6103a149d;
con ok64 BEDOGSIG  = 0x2ce35841c490;

// =====================================================================
// BESubsHere — one-level enumeration off <wt_root>/.gitmodules.
// =====================================================================

typedef struct {
    u8cs     wt_root;
    besub_cb cb;
    void    *ctx;
    ok64     err;     // sticky cb error
} besubs_ctx;

static ok64 besubs_parse_cb(u8cs path, u8cs url, void *vctx) {
    besubs_ctx *bc = (besubs_ctx *)vctx;
    if (bc->err != OK) return bc->err;

    besub s = {};
    u8csMv(s.path, path);
    u8csMv(s.url,  url);
    s.mounted = SNIFFSubIsMount(bc->wt_root, path);
    ok64 r = bc->cb(&s, bc->ctx);
    if (r != OK) bc->err = r;
    return r;
}

ok64 BESubsHere(u8cs wt_root, besub_cb cb, void *ctx) {
    sane(cb);
    if (u8csEmpty(wt_root)) done;

    //  <wt_root>/.gitmodules absent → no subs.  Use a NUL-terminated
    //  path8b for FILE I/O.
    a_path(gm_path);
    call(PATHu8bFeed, gm_path, wt_root);
    a_cstr(gm_name, ".gitmodules");
    call(PATHu8bPush, gm_path, gm_name);

    u8bp gm_map = NULL;
    ok64 mo = FILEMapRO(&gm_map, $path(gm_path));
    if (mo != OK || gm_map == NULL) done;     //  absent → empty enum

    u8cs blob = {u8bDataHead(gm_map), u8bIdleHead(gm_map)};
    besubs_ctx bc = {
        .cb  = cb,
        .ctx = ctx,
        .err = OK,
    };
    u8csMv(bc.wt_root, wt_root);
    ok64 po = SUBSu8sParse(blob, besubs_parse_cb, &bc);
    FILEUnMap(gm_map);
    if (bc.err != OK) return bc.err;
    return po;
}

// =====================================================================
// BERecurseInto — fork + chdir(<wt>/<subpath>) + execvp(self).
// =====================================================================

//  Resolve /proc/self/exe; on success feed into `out` (NUL-terminated
//  path8b).  Used by both the recursion child and parent.  Falls back
//  silently on failure — caller's check on $path(out) catches.
static void be_resolve_self(path8b out) {
    char buf[FILE_PATH_MAX_LEN];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0) return;
    buf[n] = 0;
    a_cstr(buf_s, buf);
    (void)PATHu8bFeed(out, buf_s);
}

//  Copy `s` into `pool` followed by a NUL byte; on success, write the
//  start offset into `*off_out` and bump `pool`'s idle head.  Returns
//  NOROOM if `pool` lacks space for `s.len + 1` bytes.
static ok64 be_pool_push(u8bp pool, u8cs s, size_t *off_out) {
    sane(pool && off_out);
    size_t need = (size_t)u8csLen(s) + 1;
    if ((size_t)u8bIdleLen(pool) < need) return NOROOM;
    *off_out = (size_t)(u8bIdleHead(pool) - u8bDataHead(pool));
    if (!u8csEmpty(s)) call(u8bFeed, pool, s);
    call(u8bFeed1, pool, 0);
    done;
}

ok64 BERecurseInto(u8cs wt_root, u8cs subpath, u8css argv) {
    sane($ok(wt_root) && $ok(subpath));

    //  1. Resolve self.
    a_path(self_path);
    be_resolve_self(self_path);
    if (!u8bHasData(self_path)) {
        fprintf(stderr, "be: BERecurseInto: cannot resolve self\n");
        return BEDOGEXIT;
    }

    //  2. Compose mount path: <wt_root>/<subpath>.
    a_path(mount);
    call(PATHu8bFeed, mount, wt_root);
    call(PATHu8bAdd,  mount, subpath);

    //  3. Pack argv strings into a NUL-terminated pool; build a char*
    //  array via offsets so we can finalize after the fork.  Cap at
    //  the maximum CLI argv shape forwarded from BE.cli.c.
    enum { POOL_BYTES = 8192, MAX_ARGS = 4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS };
    a_pad(u8, pool, POOL_BYTES);
    size_t offs[MAX_ARGS];
    u32    nargs = 0;

    //  argv[0]: full path to self, so HOMEResolveSibling locates the
    //  bin dir from the child's argv[0] (matches subs_spawn_be_get).
    a_dup(u8c, self_view, u8bDataC(self_path));
    if (nargs >= MAX_ARGS) return BEDOGEXIT;
    call(be_pool_push, pool, self_view, &offs[nargs]);
    nargs++;

    $for(u8cs, ap, argv) {
        if (nargs >= MAX_ARGS) return BEDOGEXIT;
        call(be_pool_push, pool, *ap, &offs[nargs]);
        nargs++;
    }

    //  4. Materialize char** from offsets.  The base pointer is
    //  stable until u8bFree, which we don't do until after waitpid.
    char *argv_c[MAX_ARGS + 1];
    for (u32 i = 0; i < nargs; i++) {
        argv_c[i] = (char *)(u8bDataHead(pool) + offs[i]);
    }
    argv_c[nargs] = NULL;

    //  NUL-terminate the mount path for chdir.
    char const *mount_cstr = (char const *)u8bDataHead(mount);

    //  5. Fork & exec.
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "be: BERecurseInto: fork failed: %s\n",
                strerror(errno));
        return BEDOGEXIT;
    }
    if (pid == 0) {
        if (chdir(mount_cstr) != 0) {
            fprintf(stderr, "be: BERecurseInto: chdir %s: %s\n",
                    mount_cstr, strerror(errno));
            _exit(127);
        }
        execvp(argv_c[0], argv_c);
        fprintf(stderr, "be: BERecurseInto: execvp %s: %s\n",
                argv_c[0], strerror(errno));
        _exit(127);
    }

    //  6. Reap.
    int rc = 0;
    ok64 r = FILEReap(pid, &rc);
    if (r == FILESIGNAL) {
        char const *sname = strsignal(rc);
        fprintf(stderr, "be: child killed by signal %d (%s)\n",
                rc, sname ? sname : "?");
        return BEDOGSIG;
    }
    if (r != OK)   return r;
    if (rc != 0)   return BEDOGEXIT;
    done;
}
