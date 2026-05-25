//  WATCH — inotify daemon implementation.  See WATCH.h.

#include "WATCH.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "abc/B.h"
#include "abc/FILE.h"
#include "abc/FSW.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "dog/DOG.h"
#include "dog/ULOG.h"

#include "AT.h"

static volatile sig_atomic_t sniff_quit = 0;

static void watch_sighandler(int sig) {
    (void)sig;
    sniff_quit = 1;
}

static ok64 watch_write_pid(u8cs reporoot) {
    sane($ok(reporoot));
    a_cstr(pidname, "sniff.pid");
    a_path(pp, reporoot, DOG_BE_S, pidname);
    FILE *fp = fopen((char *)u8bDataHead(pp), "w");
    if (!fp) fail(SNIFFFAIL);
    fprintf(fp, "%d\n", (int)getpid());
    fclose(fp);
    done;
}

static ok64 watch_rm_pid(u8cs reporoot) {
    sane($ok(reporoot));
    a_cstr(pidname, "sniff.pid");
    a_path(pp, reporoot, DOG_BE_S, pidname);
    FILEUnLink($path(pp));
    done;
}

typedef struct { int wfd; u32 count; } watchdir_ctx;

static ok64 watch_watchdir_cb(void0p arg, path8p path) {
    watchdir_ctx *ctx = (watchdir_ctx *)arg;
    u8csc p = {u8bDataHead(path), u8bIdleHead(path)};
    ok64 o = FSWDir(ctx->wfd, p);
    if (o == OK) ctx->count++;
    return OK;
}

static ok64 watch_drain_cb(u8cs path, void *ctx) {
    (void)path; (void)ctx; return OK;
}

typedef struct {
    u8cs   reporoot;
    Bu8   *seen_dirs;    // newline-sep set of dir paths already mod'd
} watch_scan_ctx;

//  YES iff `dir` already appears in `*seen` (linear scan; the set is
//  bounded by the number of distinct dirs touched between two
//  get/post events — small in practice).
static b8 watch_dir_seen(Bu8 *seen, u8cs dir) {
    a_dup(u8c, scan, u8bData(*seen));
    while (!u8csEmpty(scan)) {
        u8cs line = {};
        if (u8csDrainLine(scan, line) != OK) break;
        if (u8csEq(line, dir)) return YES;
    }
    return NO;
}

static void watch_dir_remember(Bu8 *seen, u8cs dir) {
    u8bFeed(*seen, dir);
    u8bFeed1(*seen, '\n');
}

//  Compute the parent dir slice (with trailing '/') for `rel` into
//  `out`.  Files at the wt root use "/".
static void watch_parent_dir(u8csc rel, u8b out) {
    u8bReset(out);
    u8c const *slash_last = NULL;
    for (u8c const *p = rel[0]; p < rel[1]; p++) {
        if (*p == '/') slash_last = p;
    }
    if (slash_last) {
        u8cs parent = {rel[0], slash_last + 1};   // include trailing '/'
        u8bFeed(out, parent);
    } else {
        u8bFeed1(out, '/');                       // root marker
    }
}

//  Seed `*seen` from the .be/wtlog log: every `mod <dir/>` row whose ts
//  is past the most recent get/post/patch baseline contributes its
//  path.
static ok64 watch_seed_seen(Bu8 *seen) {
    sane(seen);
    u8bReset(*seen);
    ron60 base_ts = 0, bv = 0;
    uri bu = {};
    if (SNIFFAtBaseline(&base_ts, &bv, &bu) != OK) base_ts = 0;
    ron60 v_mod = SNIFFAtVerbMod();
    u32 n = ULOGCount(SNIFF.log_idx);
    for (u32 i = 0; i < n; i++) {
        ulogrec rec = {};
        if (ULOGRow(SNIFF.log_data, SNIFF.log_idx, i, &rec) != OK) continue;
        if (rec.ts <= base_ts) continue;
        if (rec.verb != v_mod) continue;
        u8cs path = {rec.uri.path[0], rec.uri.path[1]};
        if ($empty(path)) continue;
        if (!watch_dir_seen(seen, path)) watch_dir_remember(seen, path);
    }
    done;
}

static ok64 watch_scan_cb(void *varg, path8bp path) {
    sane(varg && path);
    watch_scan_ctx *w = (watch_scan_ctx *)varg;
    a_dup(u8c, full, u8bData(path));

    u8cs rel = {};
    if (!SNIFFRelFromFull(rel, w->reporoot, full)) return OK;
    if (SNIFFSkipMeta(rel))                         return OK;
    //  The daemon's own pidfile (`<root>/.be/sniff.pid`) is filtered
    //  by SNIFFSkipMeta above (anything under `.be/`).

    filestat fs = {};
    ok64 lo = FILELStat(&fs, full);
    if (lo == FILENOENT) return OK;    // vanished mid-walk
    if (lo != OK) return lo;             // propagate
    ron60 mtime = fs.mtime;

    //  Clean against some baseline → nothing to log.
    if (SNIFFAtKnown(mtime)) return OK;

    a_pad(u8, dirbuf, 1024);
    watch_parent_dir(rel, dirbuf);
    a_dup(u8c, dir, u8bData(dirbuf));
    if (watch_dir_seen(w->seen_dirs, dir)) return OK;

    //  Append one `mod <dir/>` row via the usual URI-struct path.
    uri urow = {};
    urow.path[0] = dir[0];
    urow.path[1] = dir[1];
    ron60 vmod = SNIFFAtVerbMod();
    if (SNIFFAtAppend(vmod, &urow) == OK) {
        watch_dir_remember(w->seen_dirs, dir);
    }
    return OK;
}

static ok64 watch_rescan(u8cs reporoot, Bu8 *seen_dirs) {
    sane($ok(reporoot) && seen_dirs);
    //  Rebuild the seen set from .be/wtlog each scan — the baseline may
    //  have advanced (get/post/patch) since the last invocation,
    //  invalidating prior `mod` rows.
    call(watch_seed_seen, seen_dirs);

    watch_scan_ctx wc = {.seen_dirs = seen_dirs};
    wc.reporoot[0] = reporoot[0];
    wc.reporoot[1] = reporoot[1];

    a_path(wp);
    u8bFeed(wp, reporoot);
    call(PATHu8bTerm, wp);
    call(FILEScan, wp,
         (FILE_SCAN)(FILE_SCAN_FILES | FILE_SCAN_LINKS | FILE_SCAN_DEEP),
         watch_scan_cb, &wc);
    done;
}

ok64 SNIFFWatch(u8cs reporoot) {
    sane(1);
    pid_t pid = fork();
    if (pid < 0) fail(SNIFFFAIL);
    if (pid > 0) {
        fprintf(stderr, "sniff: daemon pid %d\n", (int)pid);
        _exit(0);
    }
    setsid();
    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > STDERR_FILENO) close(devnull);
    }
    call(watch_write_pid, reporoot);
    struct sigaction sa = {.sa_handler = watch_sighandler};
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    int wfd = -1;
    call(FSWInit, &wfd);
    { u8csc rp = {reporoot[0], reporoot[1]}; FSWDir(wfd, rp); }
    watchdir_ctx wctx = {.wfd = wfd};
    {
        a_path(wp, reporoot);
        FILEScan(wp, (FILE_SCAN)(FILE_SCAN_DIRS | FILE_SCAN_DEEP),
                 watch_watchdir_cb, &wctx);
    }

    //  Newline-sep set of directories whose `mod <dir/>` row has
    //  already been written since the most recent baseline.  Rebuilt
    //  per scan in watch_rescan().
    Bu8 seen_dirs = {};
    call(u8bAllocate, seen_dirs, 1UL << 16);

    //  Seed scan: emit mod rows for anything already dirty when the
    //  daemon starts.
    (void)watch_rescan(reporoot, &seen_dirs);

    while (!sniff_quit) {
        ok64 o = FSWPoll(wfd, 1000);
        if (o != OK) continue;
        FSWDrain(wfd, watch_drain_cb, NULL);
        (void)watch_rescan(reporoot, &seen_dirs);
    }

    u8bFree(seen_dirs);
    FSWClose(wfd);
    watch_rm_pid(reporoot);
    done;
}

ok64 SNIFFWatchStop(u8cs reporoot) {
    sane($ok(reporoot));
    a_cstr(pidname, "sniff.pid");
    a_path(pp, reporoot, DOG_BE_S, pidname);
    FILE *fp = fopen((char *)u8bDataHead(pp), "r");
    if (!fp) { fprintf(stderr, "sniff: no daemon running\n"); done; }
    int dpid = 0;
    if (fscanf(fp, "%d", &dpid) != 1 || dpid <= 0) {
        fclose(fp); fail(SNIFFFAIL);
    }
    fclose(fp);
    if (kill(dpid, SIGTERM) != 0) {
        FILEUnLink($path(pp)); fail(SNIFFFAIL);
    }
    fprintf(stderr, "sniff: stopped pid %d\n", dpid);
    FILEUnLink($path(pp));
    done;
}
