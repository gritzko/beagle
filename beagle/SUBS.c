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

#include "abc/URI.h"        // MAX_URI_LEN
#include "dog/CLI.h"        // CLI_MAX_FLAGS / CLI_MAX_URIS
#include "dog/ULOG.h"       // ULOGu8sDrain / ulogrec
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

// =====================================================================
// `be get` sub-orchestration helpers.
// =====================================================================

ok64 BEGetKeeperSubs(u8cs query, u8bp out) {
    sane(out);
    a_pad(u8cs, args, 4);
    a_cstr(keeper_s, "keeper");
    a_cstr(subs_s,   "subs");
    a_dup(u8c, keeper_d, keeper_s);
    a_dup(u8c, subs_d,   subs_s);
    u8csbFeed1(args, keeper_d);
    u8csbFeed1(args, subs_d);

    //  `?<query>` URI via URIMake (empty scheme/auth/path/frag).
    a_pad(u8, qbuf, 256);
    u8cs empty = {NULL, NULL};
    call(URIMake, u8bIdle(qbuf), empty, empty, empty, query, empty);
    a_dup(u8c, qview, u8bData(qbuf));
    u8csbFeed1(args, qview);

    a_dup(u8cs, argv, u8csbData(args));
    return be_capture(keeper_d, argv, out);
}

ok64 BEGetSubMount(u8cs subpath, u8cs pin) {
    sane($ok(subpath) && $ok(pin));

    //  Compose `./<subpath>#<pin>` via abc/PATH (path part) + URIMake
    //  (full URI render).  PATHu8bPush handles the `./` prefix and
    //  segment join; URIMake fills the fragment cleanly.
    a_path(path_buf);
    a_cstr(dot, ".");
    call(PATHu8bFeed, path_buf, dot);
    call(PATHu8bAdd,  path_buf, subpath);

    a_pad(u8, uri_buf, MAX_URI_LEN);
    u8cs empty = {NULL, NULL};
    call(URIMake, u8bIdle(uri_buf), empty, empty,
                  u8bDataC(path_buf), empty, pin);
    a_dup(u8c, uri_view, u8bData(uri_buf));

    a_pad(u8cs, args, 4);
    a_cstr(sniff_s, "sniff");
    a_cstr(verb_s,  "sub-mount");
    a_dup(u8c, sniff_d, sniff_s);
    a_dup(u8c, verb_d,  verb_s);
    u8csbFeed1(args, sniff_d);
    u8csbFeed1(args, verb_d);
    u8csbFeed1(args, uri_view);
    a_dup(u8cs, argv, u8csbData(args));

    return BERun(sniff_d, argv, NO);
}

ok64 BEGetSubUnmount(u8cs wt_root, u8cs subpath) {
    sane($ok(wt_root) && $ok(subpath));
    a_path(anchor);
    call(PATHu8bFeed, anchor, wt_root);
    call(PATHu8bAdd,  anchor, subpath);
    a_cstr(be_s, ".be");
    call(PATHu8bPush, anchor, be_s);
    (void)FILEUnLink($path(anchor));
    fprintf(stderr, "be: get %.*s: unmounted\n",
            (int)$len(subpath), (char *)subpath[0]);
    done;
}

//  YES iff any ULOG row in `ulog` has `subpath` in its query slot.
static b8 begetsubs_has(u8cs ulog, u8cs subpath) {
    u8cs scan = {ulog[0], ulog[1]};
    for (;;) {
        ulogrec row = {};
        ok64 dr = ULOGu8sDrain(scan, &row);
        if (dr == NODATA) break;
        if (dr != OK) continue;
        if (u8csEq(row.uri.query, subpath)) return YES;
    }
    return NO;
}

ok64 BEGetDrainSubs(u8cs wt_root, u8cs subs_ulog,
                    u8cs *flag_head, u8cs *flag_term) {
    sane($ok(wt_root));
    ok64 worst = OK;
    b8 outer_emitted = NO;

    u8cs scan = {subs_ulog[0], subs_ulog[1]};
    for (;;) {
        ulogrec row = {};
        ok64 dr = ULOGu8sDrain(scan, &row);
        if (dr == NODATA) break;
        if (dr != OK) continue;

        u8cs path = {};
        u8csMv(path, row.uri.query);
        u8cs pin  = {};
        u8csMv(pin,  row.uri.fragment);
        if (u8csEmpty(path) || u8csLen(pin) != 40) continue;

        if (!outer_emitted) {
            fprintf(stderr, "be: get .\n");
            outer_emitted = YES;
        }
        fprintf(stderr, "be: get %.*s\n",
                (int)$len(path), (char *)path[0]);

        u8cs subpath_arg = {};
        u8csMv(subpath_arg, path);
        u8cs pin_arg = {};
        u8csMv(pin_arg, pin);

        b8 is_mounted = SNIFFSubIsMount(wt_root, subpath_arg);
        fprintf(stderr,
                "BE.dbg: sub path=" U8SFMT " pin=" U8SFMT
                " is_mounted=%s wt_root=" U8SFMT "\n",
                u8sFmt(subpath_arg), u8sFmt(pin_arg),
                is_mounted ? "YES" : "NO", u8sFmt(wt_root));
        {
            ok64 mr = BEGetSubMount(subpath_arg, pin_arg);
            fprintf(stderr,
                    "BE.dbg: BEGetSubMount path=" U8SFMT
                    " result=%s\n",
                    u8sFmt(subpath_arg), ok64str(mr));
            if (mr != OK) { worst = mr; continue; }
        }

        //  `?<pin>` URI via URIMake.
        a_pad(u8, pin_uri, MAX_URI_LEN);
        u8cs _empty = {NULL, NULL};
        call(URIMake, u8bIdle(pin_uri),
                      _empty, _empty, _empty, pin_arg, _empty);
        a_dup(u8c, pin_uri_view, u8bDataC(pin_uri));

        a_pad(u8cs, child_args, 4 + CLI_MAX_FLAGS * 2);
        a_cstr(get_lit, "get");
        a_dup(u8c, get_d, get_lit);
        u8csbFeed1(child_args, get_d);
        for (u8cs *fp = flag_head; fp < flag_term; fp++) {
            u8csbFeed1(child_args, *fp);
        }
        u8csbFeed1(child_args, pin_uri_view);
        a_dup(u8cs, child_argv, u8csbData(child_args));

        ok64 r = BERecurseInto(wt_root, subpath_arg, child_argv);
        if (r != OK) worst = r;
    }
    return worst;
}

ok64 BEGetDrainRemoved(u8cs wt_root, u8cs baseline_ulog,
                      u8cs target_ulog) {
    sane($ok(wt_root));
    ok64 worst = OK;
    u8cs scan = {baseline_ulog[0], baseline_ulog[1]};
    for (;;) {
        ulogrec row = {};
        ok64 dr = ULOGu8sDrain(scan, &row);
        if (dr == NODATA) break;
        if (dr != OK) continue;

        u8cs path = {};
        u8csMv(path, row.uri.query);
        if (u8csEmpty(path)) continue;
        if (begetsubs_has(target_ulog, path)) continue;
        if (!SNIFFSubIsMount(wt_root, path)) continue;
        ok64 ur = BEGetSubUnmount(wt_root, path);
        if (ur != OK) worst = ur;
    }
    return worst;
}
