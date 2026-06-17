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
#include "dog/HUNK.h"       // HUNKu8sRelay
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

//  Resolve `be` to a NUL-terminated path8b via argv[0] + PATH.
//  Portable across Linux and FreeBSD; /proc/self/exe is Linux-only
//  and absent on this build host's native procfs.  HOMEResolveSibling
//  walks PATH for a bare argv[0] and uses dirname for an absolute one,
//  falling back to the bare name (which execvp still resolves via PATH).
//  SUBS-022: HOMEResolveSibling itself absolutizes a RELATIVE argv0
//  against the launch cwd, so the derived bin dir survives the recursion
//  `chdir(<wt>/<subpath>)` below.
static void be_resolve_self(path8b out) {
    a$rg(a0, 0);
    a_cstr(self_name, "be");
    (void)HOMEResolveSibling(out, self_name, a0);
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
// BEIndexMount — index a freshly-mounted sub's checked-out tree.
// =====================================================================

//  fork + chdir(<wt>/<subpath>) + execvp(<dog>, [<dog>, get]) — run
//  one indexer dog (spot / graf) against the sub mount's current tip.
//  Bare `<dog> get` (no URI) reads the mount's own `.be/wtlog` tail to
//  pick the tip, so no `--at` plumbing is needed here.  stderr is
//  inherited (quiet on success); stdout is left on the inherited fd
//  (indexers don't print on `get`).  Used by BEIndexMount.
static ok64 be_index_mount_dog(u8cs wt_root, u8cs subpath, u8csc dog) {
    sane($ok(wt_root) && $ok(subpath));

    a_path(dogpath);
    a$rg(a0, 0);
    HOMEResolveSibling(dogpath, dog, a0);
    if (!u8bHasData(dogpath)) return BEDOGEXIT;

    a_path(mount);
    call(PATHu8bFeed, mount, wt_root);
    call(PATHu8bAdd,  mount, subpath);
    char const *mount_cstr = (char const *)u8bDataHead(mount);

    //  argv: [<dog>, "get", NULL].  argv[0] = resolved dog path so the
    //  child's HOMEResolveSibling finds its own bin dir.
    char *dog_cstr = (char *)u8bDataHead(dogpath);
    char  verb_c[4] = {'g', 'e', 't', 0};
    char *argv_c[3] = {dog_cstr, verb_c, NULL};

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "be: BEIndexMount: fork failed: %s\n",
                strerror(errno));
        return BEDOGEXIT;
    }
    if (pid == 0) {
        if (chdir(mount_cstr) != 0) {
            fprintf(stderr, "be: BEIndexMount: chdir %s: %s\n",
                    mount_cstr, strerror(errno));
            _exit(127);
        }
        execvp(argv_c[0], argv_c);
        fprintf(stderr, "be: BEIndexMount: execvp %s: %s\n",
                argv_c[0], strerror(errno));
        _exit(127);
    }

    int rc = 0;
    ok64 r = FILEReap(pid, &rc);
    if (r == FILESIGNAL) return BEDOGSIG;
    if (r != OK)         return r;
    if (rc != 0)         return BEDOGEXIT;
    done;
}

ok64 BEIndexMount(u8cs wt_root, u8cs subpath) {
    sane($ok(wt_root) && $ok(subpath));
    //  Index spot first (search) then graf (history); a failure in one
    //  is reported but does not abort the other — best-effort, like the
    //  POST post-pass reindex.  Sub content is never re-checked-out, so
    //  an indexing miss only degrades search recall, never correctness.
    ok64 worst = OK;
    a_cstr(spot_s, "spot");
    a_cstr(graf_s, "graf");
    a_dup(u8c, spot_d, spot_s);
    a_dup(u8c, graf_d, graf_s);
    ok64 sr = be_index_mount_dog(wt_root, subpath, spot_d);
    if (sr != OK) worst = sr;
    ok64 gr = be_index_mount_dog(wt_root, subpath, graf_d);
    if (gr != OK) worst = gr;
    return worst;
}

// =====================================================================
// BERelaySub — capture a sub's TLV report, relay it with path prefix.
// =====================================================================

//  fork + chdir(<wt>/<subpath>) + execvp(self, argv) with the child's
//  stdout piped into `out` (reset on entry).  Mirror of BERecurseInto
//  but capturing instead of inheriting stdout (à la begetsubs_capture).
//  Reads the pipe to EOF *before* reaping, so `out` holds the child's
//  full report even when it exits non-zero.
static ok64 be_recurse_capture(u8cs wt_root, u8cs subpath, u8css argv,
                               u8bp out) {
    sane($ok(wt_root) && $ok(subpath) && out);
    u8bReset(out);

    a_path(self_path);
    be_resolve_self(self_path);
    if (!u8bHasData(self_path)) {
        fprintf(stderr, "be: be_recurse_capture: cannot resolve self\n");
        return BEDOGEXIT;
    }

    a_path(mount);
    call(PATHu8bFeed, mount, wt_root);
    call(PATHu8bAdd,  mount, subpath);

    enum { POOL_BYTES = 8192, MAX_ARGS = 4 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS };
    a_pad(u8, pool, POOL_BYTES);
    size_t offs[MAX_ARGS];
    u32    nargs = 0;

    a_dup(u8c, self_view, u8bDataC(self_path));
    if (nargs >= MAX_ARGS) return BEDOGEXIT;
    call(be_pool_push, pool, self_view, &offs[nargs]);
    nargs++;
    $for(u8cs, ap, argv) {
        if (nargs >= MAX_ARGS) return BEDOGEXIT;
        call(be_pool_push, pool, *ap, &offs[nargs]);
        nargs++;
    }

    char *argv_c[MAX_ARGS + 1];
    for (u32 i = 0; i < nargs; i++)
        argv_c[i] = (char *)(u8bDataHead(pool) + offs[i]);
    argv_c[nargs] = NULL;

    char const *mount_cstr = (char const *)u8bDataHead(mount);

    int pfd[2];
    if (pipe(pfd) != 0) {
        fprintf(stderr, "be: be_recurse_capture: pipe: %s\n", strerror(errno));
        return BEDOGEXIT;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pfd[0]); close(pfd[1]);
        fprintf(stderr, "be: be_recurse_capture: fork: %s\n", strerror(errno));
        return BEDOGEXIT;
    }
    if (pid == 0) {
        close(pfd[0]);
        if (dup2(pfd[1], STDOUT_FILENO) < 0) _exit(127);
        close(pfd[1]);
        if (chdir(mount_cstr) != 0) {
            fprintf(stderr, "be: be_recurse_capture: chdir %s: %s\n",
                    mount_cstr, strerror(errno));
            _exit(127);
        }
        execvp(argv_c[0], argv_c);
        fprintf(stderr, "be: be_recurse_capture: execvp %s: %s\n",
                argv_c[0], strerror(errno));
        _exit(127);
    }

    close(pfd[1]);
    for (;;) {
        if (u8bIdleLen(out) == 0) {
            fprintf(stderr,
                    "be: %.*s: report truncated (capture buffer full)\n",
                    (int)$len(subpath), (char *)subpath[0]);
            break;
        }
        u8 *idle = u8bIdleHead(out);
        size_t cap = (size_t)u8bIdleLen(out);
        ssize_t n = read(pfd[0], idle, cap);
        if (n < 0) { if (errno == EINTR) continue; break; }
        if (n == 0) break;
        u8bFed(out, (u32)n);
    }
    close(pfd[0]);

    int rc = 0;
    ok64 r = FILEReap(pid, &rc);
    if (r == FILESIGNAL) return BEDOGSIG;
    if (r != OK)         return r;
    //  A child `*NONE` exit (PUTNONE / DELNONE / POSTNONE — all share
    //  the NONE low byte) means the sub had nothing to do.  That's a
    //  no-op, not a failure of the parent's aggregation, so it must not
    //  abort a bare `be put` / `be delete`.  Surface it as the NONE
    //  class (not collapsed to OK) so the put/delete relay can tell a
    //  clean sub apart from one that staged real work (SUBS-004); the
    //  get/head/post recursion callers explicitly map NONE → no-op, so
    //  their exit is unchanged.
    if (rc == (int)(NONE & 0xFFu)) return NONE;
    if (rc != 0)                   return BEDOGEXIT;
    done;
}

ok64 BERelaySub(u8cs wt_root, u8cs subpath, u8css argv) {
    sane($ok(wt_root) && $ok(subpath));

    //  Build the child argv with `--tlv` forced on, so the child emits
    //  a capturable TLV hunk stream regardless of the parent's render
    //  mode.  CLISetHUNKMode lets `--tlv` win over any forwarded
    //  `--color` / `--plain`, so appending it last is safe.
    a_pad(u8cs, cargs, 8 + CLI_MAX_FLAGS * 2 + CLI_MAX_URIS);
    $for(u8cs, ap, argv) { u8csbFeed1(cargs, *ap); }
    a_cstr(tlv_lit, "--tlv");
    a_dup(u8c, tlv_d, tlv_lit);
    u8csbFeed1(cargs, tlv_d);
    a_dup(u8cs, cargv, u8csbData(cargs));

    //  Capture the child's report, then relay it even if the child
    //  exited non-zero (a PATCH conflict still lists the touched
    //  files).  The child's status is propagated after the relay.
    a_carve(u8, capbuf, 1UL << 20);
    try(be_recurse_capture, wt_root, subpath, cargv, capbuf);
    ok64 child_rc = __;

    a_dup(u8c, captured, u8bData(capbuf));
    if (!$empty(captured)) {
        a_carve(u8, obuf, 1UL << 20);
        call(HUNKu8sRelay, u8bIdle(obuf), subpath, captured);
        a_dup(u8c, relayed, u8bData(obuf));
        if (!$empty(relayed)) {
            fflush(stdout);
            size_t len = (size_t)$len(relayed);
            ssize_t w = write(STDOUT_FILENO, relayed[0], len);
            (void)w;
        }
    }
    return child_rc;
}

// =====================================================================
// `be get` sub-orchestration helpers.
// =====================================================================

//  Spawn `tool` with `argv`, drain its stdout into `out` (reset on
//  entry), reap.  Translates exit into OK / BEDOGEXIT / BEDOGSIG.
//  Used by BEGetKeeperSubs to capture `keeper subs` ULOG output.
static ok64 begetsubs_capture(u8csc tool, u8css argv, u8bp out) {
    sane($ok(tool) && out);
    u8bReset(out);
    a_path(path);
    a$rg(a0, 0);
    HOMEResolveSibling(path, tool, a0);

    int stdout_r = -1;
    pid_t pid = 0;
    call(FILESpawn, $path(path), argv, NULL, &stdout_r, &pid);

    for (;;) {
        if (u8bIdleLen(out) == 0) break;
        u8 *idle = u8bIdleHead(out);
        size_t cap = (size_t)u8bIdleLen(out);
        ssize_t n = read(stdout_r, idle, cap);
        if (n < 0) { if (errno == EINTR) continue; break; }
        if (n == 0) break;
        u8bFed(out, (u32)n);
    }
    close(stdout_r);

    int rc = 0;
    ok64 r = FILEReap(pid, &rc);
    if (r == FILESIGNAL) return BEDOGSIG;
    if (r != OK)         return r;
    if (rc != 0)         return BEDOGEXIT;
    done;
}

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
    return begetsubs_capture(keeper_d, argv, out);
}

ok64 BEGetSubMount(u8cs subpath, u8cs pin, u8cs src_uri) {
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

    //  GET-011: forward the in-flight `be get` source URI as
    //  `--source <src_uri>` so sniff's SubMount builds the PRIMARY
    //  sub-fetch candidate from the SAME remote.  Omitted (no flag)
    //  when empty — a git-source parent keeps git's own recursion.
    a_pad(u8cs, args, 6);
    a_cstr(sniff_s, "sniff");
    a_cstr(verb_s,  "sub-mount");
    a_dup(u8c, sniff_d, sniff_s);
    a_dup(u8c, verb_d,  verb_s);
    u8csbFeed1(args, sniff_d);
    u8csbFeed1(args, verb_d);
    if (!u8csEmpty(src_uri)) {
        a_cstr(src_flag, "--source");
        a_dup(u8c, src_flag_d, src_flag);
        a_dup(u8c, src_uri_d,  src_uri);
        u8csbFeed1(args, src_flag_d);
        u8csbFeed1(args, src_uri_d);
    }
    u8csbFeed1(args, uri_view);
    a_dup(u8cs, argv, u8csbData(args));

    return BERun(sniff_d, argv, NO);
}

ok64 BEGetSubUnmount(u8cs wt_root, u8cs subpath) {
    sane($ok(wt_root) && $ok(subpath));

    //  Mount dir <wt_root>/<subpath>.
    a_path(mount);
    call(PATHu8bFeed, mount, wt_root);
    call(PATHu8bAdd,  mount, subpath);

    //  1. Drop the `.be` anchor — the *logical* unmount.
    a_path(anchor);
    call(PATHu8bFeed, anchor, wt_root);
    call(PATHu8bAdd,  anchor, subpath);
    a_cstr(be_s, ".be");
    call(PATHu8bPush, anchor, be_s);
    (void)FILEUnLink($path(anchor));

    //  2. SUBS-008: the gitlink (and `.gitmodules` entry) are gone in
    //  the target tree, so the whole mount is being removed — not just
    //  the anchor.  Remove the sub's checked-out worktree files and
    //  internal metadata (`..be.idx`, `core.c`, …) too, mirroring
    //  `git submodule deinit` + the tree-switch that drops the path.
    //  The sub's objects live in the parent shard (see Submodules.mkd),
    //  so the on-disk copy under the parent wt is pure clutter once the
    //  gitlink is gone.  `FILErmrf` is depth-first, so the now-empty
    //  mount dir collapses too.  Best-effort: the logical unmount above
    //  already succeeded; a leftover-file failure must not abort GET.
    (void)FILErmrf($path(mount));

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

//  SUBS-018: does a path-scoped get reach this sub mount?  Empty scope =
//  whole-tree get (every sub).  Else the scope must BE the mount or lie
//  under it (`<mount>/…`), so `be get graf/DAG.c` reaches no `abc/` sub.
static b8 subs_scope_reaches(u8cs scope, u8cs mount) {
    if (u8csEmpty(scope))     return YES;
    if (u8csEq(scope, mount)) return YES;
    a_pad(u8, ms, FILE_PATH_MAX_LEN);
    (void)u8bFeed(ms, mount);
    (void)u8bFeed1(ms, '/');
    a_dup(u8c, ms_v, u8bDataC(ms));
    return u8csHasPrefix(scope, ms_v);
}

ok64 BEGetDrainSubs(u8cs wt_root, u8cs subs_ulog,
                    u8cs *flag_head, u8cs *flag_term, u8cs src_uri,
                    u8cs scope_path) {
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
        //  SUBS-018: skip subs the path scope does not reach.
        if (!subs_scope_reaches(scope_path, path)) continue;

        //  Pre-order recursion markers (GET-026): trace-only so default
        //  `be get` stays quiet; the relay stream (HUNKu8sRelay prefix)
        //  is the observable that proves pre-order ordering.
        if (!outer_emitted) {
            trace("be: get .\n");
            outer_emitted = YES;
        }
        trace("be: get %.*s\n",
                (int)$len(path), (char *)path[0]);

        u8cs subpath_arg = {};
        u8csMv(subpath_arg, path);
        u8cs pin_arg = {};
        u8csMv(pin_arg, pin);

        {
            ok64 mr = BEGetSubMount(subpath_arg, pin_arg, src_uri);
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

        //  Relay (not bare-recurse) so the sub's checkout report lands
        //  in the parent's stream with each restored path prefixed by
        //  the sub's mount path.  The child still performs the actual
        //  checkout; we just capture + re-emit its report.
        ok64 r = BERelaySub(wt_root, subpath_arg, child_argv);
        if (r != OK && !ok64is(r, NONE)) worst = r;   //  NONE = sub no-op

        //  SUBS-011 cause #1: the sub was checked out via a LOCAL
        //  `be get ?<pin>`, which doesn't fire BE_PLAN_GET's transport-
        //  gated spot/graf reindex rows — so the sub shard has a
        //  keeper.idx but no `.spot.idx`.  Index it now so a repo-wide
        //  search projector (cause #2 below, BEProjector recursion) has
        //  a sub trigram index to query.  Best-effort.
        if (r == OK || ok64is(r, NONE)) {
            ok64 ir = BEIndexMount(wt_root, subpath_arg);
            if (ir != OK) worst = ir;
        }
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
