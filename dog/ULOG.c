//  ULOG — append-only URI event log.
//  See dog/ULOG.h for the API and dog/ULOG.md for the format.
//
#include "ULOG.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "abc/BUF.h"
#include "abc/FILE.h"
#include "abc/PATH.h"
#include "abc/PRO.h"
#include "abc/RAP.h"

// --- streaming primitives --------------------------------------------

ok64 ULOGu8sFeed(u8s into, ron60 ts, ron60 verb, uricp u) {
    sane(into && u);
    //  Rough capacity check.  URIutf8Feed stops with its own failure
    //  if the component data doesn't fit; here we just make sure the
    //  fixed parts (ts + verb + two tabs + '\n') have room.  Assume
    //  at least 48 idle bytes available for those.
    if ((size_t)$len(into) < 48) return BNOROOM;
    call(RONutf8sFeed, into, ts);
    call(u8sFeed1, into, '\t');
    call(RONutf8sFeed, into, verb);
    call(u8sFeed1, into, '\t');
    call(URIutf8Feed, into, u);
    call(u8sFeed1, into, '\n');
    done;
}

//  Advance `scan` past the next `\n` in [scan, scan[1]).  Used to
//  recover from a malformed row.
static void ulog_skip_line(u8cs scan) {
    u8cp p = scan[0];
    while (p < scan[1] && *p != '\n') p++;
    if (p < scan[1]) p++;
    scan[0] = p;
}

//  Whitespace separator: any run of SP or TAB.  RON base64 and URI
//  byte alphabets both exclude both, so the split is unambiguous.
static b8 ulog_is_ws(u8 c) { return c == ' ' || c == '\t'; }

static u8cp ulog_skip_ws(u8cp p, u8cp e) {
    while (p < e && ulog_is_ws(*p)) p++;
    return p;
}

static u8cp ulog_find_ws(u8cp p, u8cp e) {
    while (p < e && !ulog_is_ws(*p)) p++;
    return p;
}

ok64 ULOGu8sDrain(u8cs scan,
                  ron60 *ts_out, ron60 *verb_out, urip u_out) {
    sane(scan && ts_out && verb_out && u_out);

    //  Need a terminating '\n' to have a complete row.
    u8cp nl = scan[0];
    while (nl < scan[1] && *nl != '\n') nl++;
    if (nl == scan[1]) return NODATA;

    u8cp p = scan[0];
    u8cp e = nl;

    //  ts = first token.
    u8cp ts_beg = p;
    u8cp ts_end = ulog_find_ws(ts_beg, e);
    if (ts_end == ts_beg || ts_end == e) {
        ulog_skip_line(scan);
        fail(ULOGBADFMT);
    }
    u8cs ts_str = {ts_beg, ts_end};
    ron60 ts = 0;
    a_dup(u8c, ts_dup, ts_str);
    if (RONutf8sDrain(&ts, ts_dup) != OK) {
        ulog_skip_line(scan);
        fail(ULOGBADFMT);
    }

    //  verb = second token.
    u8cp vb_beg = ulog_skip_ws(ts_end, e);
    u8cp vb_end = ulog_find_ws(vb_beg, e);
    if (vb_beg == e || vb_end == vb_beg || vb_end == e) {
        ulog_skip_line(scan);
        fail(ULOGBADFMT);
    }
    u8cs verb_str = {vb_beg, vb_end};
    ron60 verb = 0;
    a_dup(u8c, verb_dup, verb_str);
    if (RONutf8sDrain(&verb, verb_dup) != OK) {
        ulog_skip_line(scan);
        fail(ULOGBADFMT);
    }

    //  uri = rest of line (URIs disallow SP/TAB per RFC 3986).  Hand
    //  it to URILexer — component slices will point into `scan`'s
    //  backing bytes.
    u8cp uri_head = ulog_skip_ws(vb_end, e);
    u8cp uri_term = e;
    if (uri_head == uri_term) {
        ulog_skip_line(scan);
        fail(ULOGBADFMT);
    }
    memset(u_out, 0, sizeof(*u_out));
    u_out->data[0] = uri_head;
    u_out->data[1] = uri_term;
    if (URILexer(u_out) != OK) {
        ulog_skip_line(scan);
        fail(ULOGBADFMT);
    }

    *ts_out   = ts;
    *verb_out = verb;
    scan[0]   = nl + 1;
    done;
}

// --- helpers ---------------------------------------------------------

//  Scan the mmap'd text, populate the kv64 index, enforce strict
//  monotonicity.  `idx` may be NULL (skip the build).
static ok64 ulog_rebuild_idx(u8bp data, kv64bp idx) {
    sane(data);
    //  FILEBook places the whole mmap'd file content in the IDLE
    //  region (past = data = idle = map, end = map + page-aligned
    //  size).  We scan [past, end) forward, indexing each row, and
    //  then position IDLE's head past the last complete row so
    //  subsequent appends land on the right offset.  Tail zero-fill
    //  from page alignment terminates the scan at the first NUL.
    u8 *base = (u8 *)data[0];
    u8 *end  = (u8 *)data[3];
    u8cs scan = {base, end};
    u8 *last_nl_plus_one = base;
    ron60 prev = 0;
    b8 have_idx = (idx != NULL);
    while (scan[0] < scan[1]) {
        if (*scan[0] == 0) break;                             // zero-pad tail
        if (*scan[0] == '\n') {
            scan[0]++; last_nl_plus_one = (u8 *)scan[0]; continue;  // blank line
        }

        u64 off = (u64)(scan[0] - base);
        ron60 ts = 0, verb = 0;
        uri u = {};
        ok64 o = ULOGu8sDrain(scan, &ts, &verb, &u);
        if (o == NODATA) break;
        if (o != OK) return o;

        if (have_idx && kv64bDataLen(idx) > 0 && ts <= prev) fail(ULOGCLOCK);
        if (have_idx) {
            kv64 ent = {.key = ts, .val = off};
            call(kv64bPush, idx, &ent);
        }
        prev = ts;
        last_nl_plus_one = (u8 *)scan[0];
    }

    u8 **data_head = (u8 **)&data[1];
    u8 **idle_head = (u8 **)&data[2];
    *data_head = base;
    *idle_head = last_nl_plus_one;
    done;
}

//  Lower-bound on the kv64 index by timestamp key.
static u32 ulog_lower_bound(kv64b idx, ron60 ts) {
    kv64 const *a = (kv64 const *)idx[0];
    u32 n = (u32)kv64bDataLen(idx);
    u32 lo = 0, hi = n;
    while (lo < hi) {
        u32 mid = lo + (hi - lo) / 2;
        if (a[mid].key < ts) lo = mid + 1;
        else                 hi = mid;
    }
    return lo;
}

// --- public API ------------------------------------------------------

ok64 ULOGOpenBooked(u8bp *data, kv64bp idx, path8s path,
                    size_t book_size, size_t init_size) {
    sane(data && $ok(path));

    ok64 o = FILEBook(data, path, book_size);
    if (o != OK) {
        call(FILEBookCreate, data, path, book_size, init_size);
    }

    if (idx) call(kv64bAllocate, idx, 1024);

    ok64 so = ulog_rebuild_idx(*data, idx);
    if (so != OK) {
        if (idx && idx[0]) kv64bFree(idx);
        if (*data && (*data)[0]) FILEUnBook(*data);
        return so;
    }
    done;
}

ok64 ULOGOpen(u8bp *data, kv64bp idx, path8s path) {
    return ULOGOpenBooked(data, idx, path,
                          ULOG_BOOK_DEFAULT, ULOG_INIT_DEFAULT);
}

ok64 ULOGOpenRO(u8bp *data, kv64bp idx, path8s path) {
    sane(data && $ok(path));

    //  RO map only — fails if file is missing (no implicit create).
    call(FILEBookRO, data, path, ULOG_BOOK_DEFAULT);

    if (idx) call(kv64bAllocate, idx, 1024);

    ok64 so = ulog_rebuild_idx(*data, idx);
    if (so != OK) {
        if (idx && idx[0]) kv64bFree(idx);
        if (*data && (*data)[0]) FILEUnBook(*data);
        return so;
    }
    done;
}

ok64 ULOGClose(u8bp data, kv64bp idx, b8 rw) {
    sane(data);
    if (data[0]) {
        //  Any RW open page-aligned the file via FILEBook's
        //  ftruncate, so Close must trim back regardless of
        //  whether anything was actually appended.  Otherwise an
        //  early-fail RW SNIFFExec leaves trailing pad bytes
        //  visible to the next reader.  RO opens (FILEBookRO)
        //  never grew the file — skip the trim.
        if (rw) FILETrimBook(data);
        FILEUnBook(data);     // also nullifies the buffer slots
    }
    if (idx && idx[0]) kv64bFree(idx);   // also nullifies
    done;
}

ok64 ULOGAppendAt(u8bp data, kv64bp idx, ron60 ts, ron60 verb, uricp u) {
    sane(data && idx && u);
    size_t n = kv64bDataLen(idx);
    if (n > 0) {
        kv64 const *last = ((kv64 const *)idx[0]) + (n - 1);
        if (ts <= last->key) fail(ULOGCLOCK);
    }
    call(FILEBookEnsure, data, 2048);

    u64 off = (u64)u8bDataLen(data);
    call(ULOGu8sFeed, u8bIdle(data), ts, verb, u);

    kv64 ent = {.key = ts, .val = off};
    call(kv64bPush, idx, &ent);
    done;
}

ok64 ULOGAppend(u8bp data, kv64bp idx, ron60 verb, uricp u) {
    //  Clamp to max(RONNow(), tail+1) so rapid same-ms appends
    //  still land instead of tripping ULOGCLOCK.  Mirrors the
    //  pattern keeper/REFS.c uses in refs_next_ts().
    sane(data && idx);
    ron60 now = RONNow();
    size_t n = kv64bDataLen(idx);
    if (n > 0) {
        kv64 const *last = ((kv64 const *)idx[0]) + (n - 1);
        if (now <= last->key) now = last->key + 1;
    }
    return ULOGAppendAt(data, idx, now, verb, u);
}

ok64 ULOGRow(u8b data, kv64b idx, u32 i,
             ron60 *ts_out, ron60 *verb_out, urip u_out) {
    sane(data && idx && ts_out && verb_out && u_out);
    if (i >= ULOGCount(idx)) fail(ULOGNONE);
    kv64 const *a = (kv64 const *)idx[0];
    a_dup(u8c, dview, u8bDataC(data));

    u8cs scan = {dview[0] + a[i].val, dview[1]};
    return ULOGu8sDrain(scan, ts_out, verb_out, u_out);
}

ok64 ULOGSeek(kv64b idx, ron60 ts, u32 *i_out) {
    sane(idx && i_out);
    *i_out = ulog_lower_bound(idx, ts);
    done;
}

ok64 ULOGFind(kv64b idx, ron60 ts, u32 *i_out) {
    sane(idx && i_out);
    u32 i = ulog_lower_bound(idx, ts);
    u32 n = ULOGCount(idx);
    if (i >= n) fail(ULOGNONE);
    kv64 const *a = (kv64 const *)idx[0];
    if (a[i].key != ts) fail(ULOGNONE);
    *i_out = i;
    done;
}

ok64 ULOGFindLatest(u8b data, kv64b idx, ulog_pred pred, void *ctx,
                    ron60 *ts_out, urip u_out) {
    sane(data && idx && pred && ts_out && u_out);
    u32 n = ULOGCount(idx);
    for (u32 i = n; i > 0; ) {
        i--;
        ron60 ts = 0, verb = 0;
        uri u = {};
        ok64 o = ULOGRow(data, idx, i, &ts, &verb, &u);
        if (o != OK) return o;
        if (pred(&u, ctx)) {
            *ts_out = ts;
            *u_out  = u;
            done;
        }
    }
    fail(ULOGNONE);
}

ok64 ULOGFindVerb(u8b data, kv64b idx, ron60 verb,
                  ron60 *ts_out, urip u_out) {
    sane(data && idx && ts_out && u_out);
    u32 n = ULOGCount(idx);
    for (u32 i = n; i > 0; ) {
        i--;
        ron60 ts = 0, v = 0;
        uri u = {};
        ok64 o = ULOGRow(data, idx, i, &ts, &v, &u);
        if (o != OK) return o;
        if (v == verb) {
            *ts_out = ts;
            *u_out  = u;
            done;
        }
    }
    fail(ULOGNONE);
}

// --- latest-per-key helpers ------------------------------------------

//  Compute the key slice for row `i`: the URI bytes up to (but not
//  including) the first `#`.  Walks the raw row text in the mmap —
//  O(row length) — avoiding URIutf8Feed's component reassembly.
static void ulog_row_key_bytes(u8b data, kv64b idx, u32 i, u8csp out) {
    kv64 const *a = (kv64 const *)idx[0];
    u8cp base = (u8cp)data[0];
    u8cp row  = base + a[i].val;
    u8cp end  = (u8cp)data[2];  // idle head = first byte past data
    u8cp nl = row;
    while (nl < end && *nl != '\n') nl++;

    //  Skip two tab/space-separated tokens (ts, verb) to reach the URI.
    u8cp p = row;
    int tok = 0;
    while (tok < 2 && p < nl) {
        while (p < nl && *p != '\t' && *p != ' ') p++;
        while (p < nl && (*p == '\t' || *p == ' ')) p++;
        tok++;
    }
    u8cp uri_begin = p;
    //  Key ends at the first '#' or at end-of-line.
    u8cp h = uri_begin;
    while (h < nl && *h != '#') h++;

    out[0] = uri_begin;
    out[1] = h;
}

//  Linear-scan "seen" set of u64 hashes.  For N ≤ a few thousand
//  unique keys the O(N²) total is trivially fast; bigger logs would
//  want a proper hash table (HASHx).  Documented in ULOG.md.
static b8 seen_contains(Bu64 seen, u64 h) {
    u64 const *a = (u64 const *)seen[0];
    u32 n = (u32)u64bDataLen(seen);
    for (u32 j = 0; j < n; j++) if (a[j] == h) return YES;
    return NO;
}

ok64 ULOGeachLatest(u8b data, kv64b idx, ron60 verb_filter,
                    ulog_each_fn cb, void *ctx) {
    sane(data && idx && cb);
    u32 n = ULOGCount(idx);
    if (n == 0) done;

    Bu64 seen = {};
    call(u64bAllocate, seen, 1024);

    ok64 rc = OK;
    for (u32 i = n; i > 0; ) {
        i--;
        ron60 ts = 0, verb = 0;
        uri u = {};
        ok64 o = ULOGRow(data, idx, i, &ts, &verb, &u);
        if (o != OK) { rc = o; break; }
        if (verb_filter && verb != verb_filter) continue;

        u8cs key = {};
        ulog_row_key_bytes(data, idx, i, key);
        //  Dedup key includes verb: seed the hash with verb so two
        //  different verbs over the same URI-minus-fragment hash to
        //  different slots.
        u64 h = RAPHashSeed(key, (u64)verb);
        if (seen_contains(seen, h)) continue;
        u64bFeed1(seen, h);

        rc = cb(ts, verb, &u, ctx);
        if (rc != OK) break;
    }

    u64bFree(seen);
    return rc;
}

// --- ULOGCompactLatest -----------------------------------------------

//  Two-pass compaction: mark keep-bits on a reverse walk, then append
//  kept rows forward into a fresh tmp log and rename over the original.
ok64 ULOGCompactLatest(u8bp *data, kv64bp idx, path8s path,
                       ron60 verb_filter) {
    sane(data && idx && $ok(path));

    u32 n = ULOGCount(idx);
    if (n == 0) done;

    b8 *keep = (b8 *)calloc((size_t)n, 1);
    if (!keep) fail(ULOGFAIL);

    Bu64 seen = {};
    ok64 allo = u64bAllocate(seen, 1024);
    if (allo != OK) { free(keep); return allo; }

    //  Pass 1: reverse walk, mark rows that survive compaction.
    for (u32 i = n; i > 0; ) {
        i--;
        ron60 ts = 0, verb = 0;
        uri u = {};
        ok64 o = ULOGRow(*data, idx, i, &ts, &verb, &u);
        if (o != OK) { u64bFree(seen); free(keep); return o; }

        if (verb_filter && verb != verb_filter) {
            keep[i] = 1;
            continue;
        }
        u8cs key = {};
        ulog_row_key_bytes(*data, idx, i, key);
        //  Dedup key includes verb: seed the hash with verb so two
        //  different verbs over the same URI-minus-fragment hash to
        //  different slots.
        u64 h = RAPHashSeed(key, (u64)verb);
        if (seen_contains(seen, h)) continue;
        u64bFeed1(seen, h);
        keep[i] = 1;
    }
    u64bFree(seen);

    //  Build `<path>.tmp`.
    a_pad(u8, tmp_buf, FILE_PATH_MAX_LEN);
    u8bFeed(tmp_buf, path);
    a_cstr(tmp_suffix, ".tmp");
    if (u8bFeed(tmp_buf, tmp_suffix) != OK) { free(keep); fail(ULOGFAIL); }
    if (PATHu8bTerm(tmp_buf) != OK) { free(keep); fail(ULOGFAIL); }
    a_dup(u8c, tmp_path, u8bDataC(tmp_buf));
    (void)unlink((char const *)tmp_path[0]);

    u8bp  tmp_data = NULL;
    Bkv64 tmp_idx  = {};
    ok64 oo = ULOGOpen(&tmp_data, tmp_idx, tmp_path);
    if (oo != OK) { free(keep); return oo; }

    //  Pass 2: forward walk, re-append kept rows into tmp.
    for (u32 i = 0; i < n; i++) {
        if (!keep[i]) continue;
        ron60 ts = 0, verb = 0;
        uri u = {};
        ok64 o = ULOGRow(*data, idx, i, &ts, &verb, &u);
        if (o != OK) {
            ULOGClose(tmp_data, tmp_idx, YES);
            free(keep); return o;
        }
        ok64 ao = ULOGAppendAt(tmp_data, tmp_idx, ts, verb, &u);
        if (ao != OK) {
            ULOGClose(tmp_data, tmp_idx, YES);
            free(keep); return ao;
        }
    }
    free(keep);
    ULOGClose(tmp_data, tmp_idx, YES);

    //  Swap files: close source, rename tmp → path, reopen source.
    ULOGClose(*data, idx, YES);
    ok64 ro = FILERename(tmp_path, path);
    if (ro != OK) {
        //  Best-effort recovery: try to reopen the original (still intact).
        (void)ULOGOpen(data, idx, path);
        return ro;
    }
    call(ULOGOpen, data, idx, path);
    done;
}

ok64 ULOGTruncate(u8bp data, kv64bp idx, u32 keep_n) {
    sane(data && idx);
    u32 n = ULOGCount(idx);
    if (keep_n > n) fail(ULOGFAIL);
    if (keep_n == n) done;

    u64 cut_off = 0;
    if (keep_n > 0) {
        kv64 const *a = (kv64 const *)idx[0];
        cut_off = a[keep_n].val;
    }

    //  Shrink the kv64 index to `keep_n` entries.
    kv64 **idle_idx = kv64bIdle(idx);
    kv64 *base = (kv64 *)idx[0];
    *idle_idx = base + keep_n;

    //  Drop the tail bytes of the data book.  Zero the discarded
    //  region so a reopen's `ulog_rebuild_idx` halts at the first NUL;
    //  we deliberately do NOT ftruncate — keeping the backing file
    //  page-aligned is what lets subsequent MAP_SHARED writes land on
    //  strict filesystems (ext4 silently drops writes past EOF even
    //  within a page-aligned mmap).
    u8 *data_base  = (u8 *)data[0];
    u8 **data_idle = u8bIdle(data);
    u8 *old_idle   = *data_idle;
    u8 *new_idle   = data_base + cut_off;
    if (new_idle < old_idle) memset(new_idle, 0, (size_t)(old_idle - new_idle));
    *data_idle = new_idle;

    done;
}
