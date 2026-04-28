//  AT — sniff's attribution log, layered over dog/ULOG.
//
#include "AT.h"
#include "SNIFF.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "dog/QURY.h"
#include "keeper/WALK.h"   // WALK_KIND_*

//  Row-0 invariant guard: `repo` only at row 0, every other verb only
//  at row ≥ 1.  Returns OK if the append is allowed.
static ok64 at_check_row0(ron60 verb) {
    sane(SNIFF.h);
    ron60 vrepo = SNIFFAtVerbRepo();
    u32 n = ULOGCount(SNIFF.log_idx);
    if (n == 0 && verb != vrepo) fail(SNIFFFAIL);
    if (n > 0 && verb == vrepo)  fail(SNIFFFAIL);
    done;
}

ok64 SNIFFAtAppend(ron60 verb, uricp u) {
    sane(SNIFF.h && u);
    call(at_check_row0, verb);
    ulogrec rec = {.verb = verb, .uri = *u};
    return ULOGAppend(SNIFF.log_data, SNIFF.log_idx, &rec);
}

ok64 SNIFFAtAppendAt(ron60 ts, ron60 verb, uricp u) {
    sane(SNIFF.h && u);
    call(at_check_row0, verb);
    ulogrec rec = {.ts = ts, .verb = verb, .uri = *u};
    return ULOGAppendAt(SNIFF.log_data, SNIFF.log_idx, &rec);
}

b8 SNIFFAtKnown(ron60 mtime) {
    if (!SNIFF.h) return NO;
    return ULOGHas(SNIFF.log_idx, mtime);
}

void SNIFFAtPathBytes(uri const *u, u8cs out) {
    if (!u8csEmpty(u->path))     { out[0] = u->path[0];     out[1] = u->path[1];     return; }
    if (!u8csEmpty(u->query))    { out[0] = u->query[0];    out[1] = u->query[1];    return; }
    if (!u8csEmpty(u->fragment)) { out[0] = u->fragment[0]; out[1] = u->fragment[1]; return; }
    out[0] = u->data[0]; out[1] = u->data[1];
}

// --- Verb constants (lazy-cached) ---

static ron60 at_v_repo   = 0;
static ron60 at_v_get    = 0;
static ron60 at_v_post   = 0;
static ron60 at_v_patch  = 0;
static ron60 at_v_put    = 0;
static ron60 at_v_delete = 0;
static ron60 at_v_mod    = 0;

ron60 SNIFFAtVerbRepo(void) {
    if (at_v_repo == 0) { a_cstr(s, "repo"); at_v_repo = SNIFFAtVerbOf(s); }
    return at_v_repo;
}

ron60 SNIFFAtVerbGet(void) {
    if (at_v_get == 0) { a_cstr(s, "get"); at_v_get = SNIFFAtVerbOf(s); }
    return at_v_get;
}
ron60 SNIFFAtVerbPost(void) {
    if (at_v_post == 0) { a_cstr(s, "post"); at_v_post = SNIFFAtVerbOf(s); }
    return at_v_post;
}
ron60 SNIFFAtVerbPatch(void) {
    if (at_v_patch == 0) { a_cstr(s, "patch"); at_v_patch = SNIFFAtVerbOf(s); }
    return at_v_patch;
}
ron60 SNIFFAtVerbPut(void) {
    if (at_v_put == 0) { a_cstr(s, "put"); at_v_put = SNIFFAtVerbOf(s); }
    return at_v_put;
}
ron60 SNIFFAtVerbDelete(void) {
    if (at_v_delete == 0) { a_cstr(s, "delete"); at_v_delete = SNIFFAtVerbOf(s); }
    return at_v_delete;
}
ron60 SNIFFAtVerbMod(void) {
    if (at_v_mod == 0) { a_cstr(s, "mod"); at_v_mod = SNIFFAtVerbOf(s); }
    return at_v_mod;
}

// --- Row-0 anchor lookup ---

ok64 SNIFFAtRepo(urip u_out) {
    sane(SNIFF.h && u_out);
    if (ULOGCount(SNIFF.log_idx) == 0) return ULOGNONE;
    ulogrec rec = {};
    call(ULOGRow, SNIFF.log_data, SNIFF.log_idx, 0, &rec);
    if (rec.verb != SNIFFAtVerbRepo()) fail(SNIFFFAIL);
    *u_out = rec.uri;
    done;
}

// --- Baseline URI lookup ---

ok64 SNIFFAtBaseline(ron60 *ts_out, ron60 *verb_out, urip u_out) {
    sane(SNIFF.h && ts_out && verb_out && u_out);
    ron60 vg = SNIFFAtVerbGet();
    ron60 vp = SNIFFAtVerbPost();
    ron60 vx = SNIFFAtVerbPatch();
    u32 n = ULOGCount(SNIFF.log_idx);
    for (u32 i = n; i > 0; i--) {
        ulogrec rec = {};
        ok64 o = ULOGRow(SNIFF.log_data, SNIFF.log_idx, i - 1, &rec);
        if (o != OK) return o;
        if (rec.verb == vg || rec.verb == vp || rec.verb == vx) {
            *ts_out   = rec.ts;
            *verb_out = rec.verb;
            *u_out    = rec.uri;
            done;
        }
    }
    return ULOGNONE;
}

// --- Last-post timestamp ---

ron60 SNIFFAtLastPostTs(void) {
    if (!SNIFF.h) return 0;
    ron60 vp = SNIFFAtVerbPost();
    u32 n = ULOGCount(SNIFF.log_idx);
    for (u32 i = n; i > 0; ) {
        i--;
        ulogrec rec = {};
        if (ULOGRow(SNIFF.log_data, SNIFF.log_idx, i, &rec) != OK) return 0;
        if (rec.verb == vp) return rec.ts;
    }
    return 0;
}

// --- Put/delete forward scan since floor ---

// --- ron60 ↔ timespec helpers ---

ron60 SNIFFAtOfTimespec(struct timespec tsp) {
    struct tm tm = {};
    time_t sec = tsp.tv_sec;
    //  RONNow uses localtime, so match that for round-trip.
    localtime_r(&sec, &tm);
    u32 ms = (u32)(tsp.tv_nsec / 1000000);
    if (ms > 999) ms = 999;
    ron60 r = 0;
    RONOfTime(&r, &tm, ms);
    return r;
}

static struct timespec at_ts_of_ron60(ron60 r) {
    struct tm tm = {};
    u32 ms = 0;
    struct timespec ts = {};
    if (RONToTime(r, &tm, &ms) != OK) return ts;
    //  RONNow wrote via localtime; reverse via mktime (local tz).
    //  `tm_isdst = -1` tells mktime to auto-detect DST from the local
    //  calendar; without it mktime assumes tm_isdst=0, which shifts
    //  the computed time_t by one hour during DST, breaking the
    //  ron60 ↔ timespec round-trip at the SNIFFAtKnown check.
    tm.tm_isdst = -1;
    time_t sec = mktime(&tm);
    ts.tv_sec = sec;
    ts.tv_nsec = (long)ms * 1000000L;
    return ts;
}

void SNIFFAtNow(ron60 *ts_out, struct timespec *tv_out) {
    ron60 now = RONNow();
    //  Guard monotonicity against the ULOG tail.
    if (SNIFF.h) {
        ulogrec tail = {};
        if (ULOGTail(SNIFF.log_data, SNIFF.log_idx, &tail) == OK) {
            if (now <= tail.ts) now = tail.ts + 1;
        }
    }
    *ts_out = now;
    *tv_out = at_ts_of_ron60(now);
}

ok64 SNIFFAtRowAtTs(ron60 mtime, ron60 *verb_out, urip u_out) {
    sane(SNIFF.h && verb_out && u_out);
    u32 i = 0;
    ok64 fo = ULOGFind(SNIFF.log_idx, mtime, &i);
    if (fo != OK) return fo;
    ulogrec rec = {};
    call(ULOGRow, SNIFF.log_data, SNIFF.log_idx, i, &rec);
    *verb_out = rec.verb;
    *u_out    = rec.uri;
    done;
}

ok64 SNIFFCheckClock(void) {
    sane(1);
    if (!SNIFF.h) done;                       // no log yet, nothing to compare
    ulogrec tail = {};
    if (ULOGTail(SNIFF.log_data, SNIFF.log_idx, &tail) != OK) done;
    ron60 now = RONNow();
    if (now < tail.ts) {
        fprintf(stderr,
                "sniff: clock skew — system clock is before the latest "
                ".sniff row; refusing every command until clock catches "
                "up\n");
        return SNIFFCLOCKBAD;
    }
    done;
}

ok64 SNIFFAtStampPath(path8b path, ron60 ts) {
    sane(path);
    struct timespec tv = at_ts_of_ron60(ts);
    struct timespec times[2] = { tv, tv };
    char const *cp = (char const *)u8bDataHead(path);
    if (utimensat(AT_FDCWD, cp, times, AT_SYMLINK_NOFOLLOW) != 0) fail(SNIFFFAIL);
    done;
}

ok64 SNIFFAtScanPutDelete(ron60 floor, sniff_at_pd_cb cb, void *ctx) {
    sane(SNIFF.h && cb);
    u32 start = 0;
    ok64 s = ULOGSeek(SNIFF.log_idx, floor, &start);
    if (s != OK && s != ULOGNONE) return s;
    u32 n = ULOGCount(SNIFF.log_idx);
    ron60 vput = SNIFFAtVerbPut();
    ron60 vdel = SNIFFAtVerbDelete();
    for (u32 i = start; i < n; i++) {
        ulogrec rec = {};
        ok64 o = ULOGRow(SNIFF.log_data, SNIFF.log_idx, i, &rec);
        if (o != OK) return o;
        if (rec.ts <= floor) continue;
        if (rec.verb != vput && rec.verb != vdel) continue;
        a_dup(u8c, path, rec.uri.path);
        ok64 cr = cb(rec.verb, path, rec.ts, ctx);
        if (cr != OK) return cr;
    }
    done;
}

ok64 SNIFFAtQueryFirstSha(uricp u, u8 *out_hex40) {
    sane(u && out_hex40);

    //  Canonical at-log row: `?<branch>#<curhash>` — fragment is the
    //  current sha.  Take it directly when present.
    {
        u8cs frag = {u->fragment[0], u->fragment[1]};
        if (u8csLen(frag) == 40) {
            memcpy(out_hex40, frag[0], 40);
            done;
        }
    }

    //  Legacy rows kept the sha in the query (`?<branch>&<sha>`) —
    //  walk the `&`-chain and pick the first 40-hex spec.  Tolerate
    //  empty leading specs (`&<sha>`) by skipping bare separators.
    a_dup(u8c, q, u->query);
    while (!$empty(q)) {
        if (*q[0] == '&') { u8csUsed1(q); continue; }
        qref spec = {};
        if (QURYu8sDrain(q, &spec) != OK) break;
        if (spec.type == QURY_NONE) break;
        if (spec.type == QURY_SHA && $len(spec.body) == 40) {
            memcpy(out_hex40, spec.body[0], 40);
            done;
        }
    }
    fail(ULOGNONE);
}

// --- SNIFFAtScanDirty -------------------------------------------------

typedef struct {
    u8cs              reporoot;
    sniff_at_dirty_cb cb;
    void             *user_ctx;
    ok64              cb_err;
} at_dirty_scan_ctx;

static ok64 at_dirty_scan_cb(void *varg, path8bp path) {
    sane(varg && path);
    at_dirty_scan_ctx *c = (at_dirty_scan_ctx *)varg;

    a_dup(u8c, full, u8bData(path));
    u8cs rel = {};
    if (!SNIFFRelFromFull(&rel, c->reporoot, full)) return OK;
    if (SNIFFSkipMeta(rel))                         return OK;

    struct stat sb = {};
    if (lstat((char const *)full[0], &sb) != 0) return OK;
    struct timespec ts = {.tv_sec  = sb.st_mtim.tv_sec,
                          .tv_nsec = sb.st_mtim.tv_nsec};
    if (SNIFFAtKnown(SNIFFAtOfTimespec(ts))) return OK;

    ok64 o = c->cb(rel, c->user_ctx);
    if (o != OK) c->cb_err = o;
    return o;
}

ok64 SNIFFAtScanDirty(u8cs reporoot, sniff_at_dirty_cb cb, void *ctx) {
    sane($ok(reporoot) && cb != NULL);
    at_dirty_scan_ctx sc = {.cb = cb, .user_ctx = ctx, .cb_err = OK};
    u8csMv(sc.reporoot, reporoot);
    a_path(wp);
    u8bFeed(wp, reporoot);
    call(PATHu8bTerm, wp);
    ok64 so = FILEScan(wp,
                       (FILE_SCAN)(FILE_SCAN_FILES | FILE_SCAN_LINKS |
                                   FILE_SCAN_DEEP),
                       at_dirty_scan_cb, &sc);
    if (sc.cb_err != OK) return sc.cb_err;
    return so;
}

// --- SNIFFWtListPaths -------------------------------------------------

typedef struct {
    u8cs reporoot;
    u8bp paths;
    u8bp meta;
    ok64 err;
} at_list_ctx;

static ok64 at_list_cb(void *varg, path8bp path) {
    sane(varg && path);
    at_list_ctx *c = (at_list_ctx *)varg;

    a_dup(u8c, full, u8bData(path));
    u8cs rel = {};
    if (!SNIFFRelFromFull(&rel, c->reporoot, full)) return OK;
    if (SNIFFSkipMeta(rel))                         return OK;

    struct stat sb = {};
    if (lstat((char const *)full[0], &sb) != 0) return OK;
    u8 kind;
    if      (S_ISLNK(sb.st_mode))     kind = WALK_KIND_LNK;
    else if (sb.st_mode & S_IXUSR)    kind = WALK_KIND_EXE;
    else                              kind = WALK_KIND_REG;

    ok64 o = u8bFeed(c->paths, rel);
    if (o == OK) o = u8bFeed1(c->paths, '\n');
    if (o == OK) o = u8bFeed1(c->meta,  kind);
    if (o != OK) { c->err = o; return o; }
    return OK;
}

ok64 SNIFFWtListPaths(u8cs reporoot, u8bp out_paths, u8bp out_meta) {
    sane($ok(reporoot) && out_paths && out_meta);
    u8bReset(out_paths);
    u8bReset(out_meta);
    at_list_ctx c = {.paths = out_paths, .meta = out_meta, .err = OK};
    u8csMv(c.reporoot, reporoot);

    a_path(wp);
    u8bFeed(wp, reporoot);
    call(PATHu8bTerm, wp);

    //  FILEScanSorted needs scratch buffer for per-dir entry stacks.
    //  Sized at 1 MB — large enough for tens of thousands of entries
    //  per dir, well beyond anything reasonable.
    Bu8 scratch = {};
    call(u8bAllocate, scratch, 1UL << 20);

    ok64 so = FILEScanSorted(wp,
                             (FILE_SCAN)(FILE_SCAN_FILES | FILE_SCAN_LINKS |
                                         FILE_SCAN_DEEP),
                             scratch, FILEentryZ, at_list_cb, &c);
    u8bFree(scratch);
    if (c.err != OK) return c.err;
    return so;
}
