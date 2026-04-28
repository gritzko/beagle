#ifndef DOG_ULOG_H
#define DOG_ULOG_H

//  ULOG — append-only URI event log.
//
//  Each row is `<ron60-ms>\t<verb-ron60>\t<uri>\n`.  Both the timestamp
//  and the verb are RON-base64 encoded u64s (abc/RON.h): the verb
//  carries the CLI-shaped tag that drove the event (`get`, `put`,
//  `post`, `delete`, `patch`, `sync`, …).  Timestamps are strictly
//  monotonic across the file; a non-monotonic row (on append or while
//  scanning) is fatal (ULOGCLOCK).
//
//  Storage:
//    `u8bp  data`   pointer to the FILEBook'd buffer slot (FILE_WANT_BUFS
//                   entry).  Caller declares `u8bp data = NULL;` and
//                   passes `&data` to Open so FILEBook can fill it in.
//    `Bkv64 idx`    optional in-memory index, key = ts, val = byte
//                   offset of the row in `data`.  Naturally sorted by
//                   construction; rebuilt by one linear scan on Open.
//                   Pass NULL on Open to skip the build (streaming-
//                   only consumers); random-access functions require
//                   a non-NULL idx.
//
//  URI slices returned by ULOGRow / ULOGTail / etc. point into the
//  mmap and stay valid until ULOGClose / ULOGTruncate.
//
//  Streaming primitives (`ULOGu8sFeed`, `ULOGu8sDrain`) are stateless —
//  use them when you have no file (encoding to a wire buffer, parsing
//  a `tail -f` stream, etc).
//
//  See dog/ULOG.md for the format and design notes.

#include "abc/BUF.h"
#include "abc/KV.h"
#include "abc/OK.h"
#include "abc/PATH.h"
#include "abc/RON.h"
#include "abc/URI.h"

con ok64 ULOGFAIL   = 0x7956103ca495;
con ok64 ULOGNONE   = 0x7956105d85ce;
con ok64 ULOGCLOCK  = 0x1e55840c558314;
con ok64 ULOGBADFMT = 0x7956102ca34f59d;

//  Default book reservation — 1 GiB virtual address space, 4 KiB
//  initial file size.  Callers can tune via ULOGOpenBooked.
#define ULOG_BOOK_DEFAULT (1UL << 30)
#define ULOG_INIT_DEFAULT 4096

// --- streaming primitives (stateless) -------------------------------

//  Encode one row into `into`'s idle.  URI bytes are produced via
//  URIutf8Feed from `u`'s components.  Returns BNOROOM if idle space
//  is insufficient.  Monotonicity is the caller's responsibility.
ok64 ULOGu8sFeed(u8s into, ron60 ts, ron60 verb, uricp u);

//  Drain one complete row (through the trailing '\n') from `scan`.
//  On OK, `scan[0]` is advanced past the row, `*ts_out` / `*verb_out`
//  are filled, and URILexer is run against the row's URI slice —
//  `u_out`'s component slices point into the input buffer; `u_out->data`
//  is the consumed input.  Field separator: one or more SP/TAB bytes.
//
//  Returns:
//    OK         one row parsed, scan advanced.
//    NODATA     no complete row yet (no '\n' in scan); scan unchanged.
//    ULOGBADFMT malformed row; scan advanced past the bad line's '\n'.
ok64 ULOGu8sDrain(u8cs scan,
                  ron60 *ts_out, ron60 *verb_out, urip u_out);

// --- open / close ----------------------------------------------------

//  Open (create if missing) RW.  `idx` may be NULL (skip indexing).
//  Returns ULOGCLOCK if existing rows are not strictly monotonic,
//  ULOGBADFMT on malformed lines.
ok64 ULOGOpen(u8bp *data, kv64bp idx, path8s path);

//  Variant with explicit book sizing.
ok64 ULOGOpenBooked(u8bp *data, kv64bp idx, path8s path,
                    size_t book_size, size_t init_size);

//  Read-only open (PROT_READ via FILEBookRO).  Appends will fail.
//  Missing file is an error (no implicit create).  `idx` may be NULL.
ok64 ULOGOpenRO(u8bp *data, kv64bp idx, path8s path);

//  Close.  `rw=YES` trims the file (RW open page-aligned via FILEBook —
//  Close must trim back even if no append happened); `rw=NO` skips the
//  trim (RO opens never grew the file).  `idx` may be NULL.
//  After return, `data`'s slot is NULL'd by FILEUnBook.
ok64 ULOGClose(u8bp data, kv64bp idx, b8 rw);

// --- append ----------------------------------------------------------

//  Append a row using `RONNow()` clamped to `tail+1` ms.  Requires a
//  non-NULL idx (used for monotonicity check + offset push).
ok64 ULOGAppend  (u8bp data, kv64bp idx,           ron60 verb, uricp u);

//  Append with explicit ts.  Refuses ULOGCLOCK if `ts <= tail`.
ok64 ULOGAppendAt(u8bp data, kv64bp idx, ron60 ts, ron60 verb, uricp u);

// --- random access (require non-NULL idx) ---------------------------

//  Row count.
fun u32 ULOGCount(kv64b idx) { return (u32)kv64bDataLen(idx); }

//  Random access, 0-indexed.
ok64 ULOGRow(u8b data, kv64b idx, u32 i,
             ron60 *ts_out, ron60 *verb_out, urip u_out);

//  Lower-bound: smallest index i such that idx[i].key >= ts.
//  Writes the count if ts is past the tail.
ok64 ULOGSeek(kv64b idx, ron60 ts, u32 *i_out);

//  Exact-timestamp lookup.  ULOGNONE if no row has that stamp.
ok64 ULOGFind(kv64b idx, ron60 ts, u32 *i_out);

//  First / last row convenience (inline over ULOGRow).
fun ok64 ULOGHead(u8b data, kv64b idx,
                  ron60 *ts_out, ron60 *verb_out, urip u_out) {
    if (ULOGCount(idx) == 0) return ULOGNONE;
    return ULOGRow(data, idx, 0, ts_out, verb_out, u_out);
}

fun ok64 ULOGTail(u8b data, kv64b idx,
                  ron60 *ts_out, ron60 *verb_out, urip u_out) {
    u32 n = ULOGCount(idx);
    if (n == 0) return ULOGNONE;
    return ULOGRow(data, idx, n - 1, ts_out, verb_out, u_out);
}

//  Cheap `ts ∈ log` predicate (binary search).
fun b8 ULOGHas(kv64b idx, ron60 ts) {
    u32 i = 0;
    return ULOGFind(idx, ts, &i) == OK;
}

// --- reverse scans ---------------------------------------------------

//  Reverse scan with predicate; stops at the first row the predicate
//  accepts.  ULOGNONE if no row matches.
typedef b8 (*ulog_pred)(uricp u, void *ctx);
ok64 ULOGFindLatest(u8b data, kv64b idx, ulog_pred pred, void *ctx,
                    ron60 *ts_out, urip u_out);

//  Latest row whose verb matches `verb`.  Reverse scan; stops at first
//  match.  ULOGNONE if no row carries that verb.
ok64 ULOGFindVerb(u8b data, kv64b idx, ron60 verb,
                  ron60 *ts_out, urip u_out);

//  Iteration callback for `ULOGeachLatest`.  A non-OK return aborts
//  the walk and is propagated out.
typedef ok64 (*ulog_each_fn)(ron60 ts, ron60 verb, uricp u, void *ctx);

//  Walk in reverse chronological order, invoking `cb` at most ONCE per
//  unique (verb, URI-minus-fragment) key — always with the latest row
//  bearing that key.  `verb_filter == 0` considers every verb.
ok64 ULOGeachLatest(u8b data, kv64b idx, ron60 verb_filter,
                    ulog_each_fn cb, void *ctx);

// --- compact / truncate ---------------------------------------------

//  Rewrite the log keeping only the latest row per (verb,
//  URI-minus-fragment) key.  Atomic via `<path>.tmp` + rename(2).
//  On success `*data`/`idx` are reopened against the compacted file.
ok64 ULOGCompactLatest(u8bp *data, kv64bp idx, path8s path,
                       ron60 verb_filter);

//  Compaction — keep rows [0, keep_n), discard the rest.
ok64 ULOGTruncate(u8bp data, kv64bp idx, u32 keep_n);

#endif
