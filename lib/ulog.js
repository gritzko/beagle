//  ulog.js — the single ULOG read+write module shared by bin/*.js (JS-043,
//  JS-048).  Pure JS over the JABC ULOG family (`abc.mmap("ULOG", path, "r")`
//  for reads; `abc.ram("ULOG", …)` + io leaves for the crash-safe write).
//  libabc+libdog ONLY — no keeper/sniff binding; we append the ULOG file
//  ourselves.  Consolidates the watermark-fix-up drain incantation that was
//  open-coded ~6× (wtlog.js, keeper.js, be.js, get.js, ingest.js) AND the
//  crash-safe, monotonic ULOG writer (folded in from wtwrite.js).
//
//  An RO-mapped ULOG opens with watermark 0 (the write head); the read
//  cursor (next/rewind) treats watermark as the DATA length, so every
//  reader must first set watermark = byteLength to expose the whole file
//  (the JS-029 finding).  `each`/`drain` bottle that up:
//
//  READERS
//    each(path, cb)  open RO, expose all rows, call cb(log) per row in
//                    frame (rule #4 — no held native cursor); a failed
//                    open is a silent no-op (matches every former site's
//                    try/catch).  The caller pulls log.time/verb/uri.
//    drain(path)  →  [{ ts, ron, verb, uri:URI }]  the rich row list
//                    (the wtlog.js / keeper.js drainUlog shape).
//
//  WRITERS (the wtlog/ULOG WRITE substrate; the write twin of the readers)
//    write(path, rows)      build a fresh ULOG from `rows` and write it
//                           CRASH-SAFELY (temp file + io.rename); each row
//                           is { verb, uri, ts? } — an explicit ts is honoured.
//    append(bePath, rows)   read the existing ULOG, sample a ts STRICTLY
//                           greater than its tail (SNIFFAtNow's monotonic
//                           bump; a gross-backwards clock throws CLOCKBAD),
//                           feed the new rows with explicit increasing ts,
//                           and rewrite via write().
//    _stage(path, rows)     the crash-safe half: write a temp sibling and
//                           return its path WITHOUT renaming (caller renames).
//
//  Row shapes are the caller's (sniff/AT.md): wt rows `<verb> ?<branch>#<sha>`,
//  refs rows `<verb> ?<key>#<sha>`.  The write side only owns ts assignment +
//  the durable write.

"use strict";

//  --- READERS ------------------------------------------------------------

function each(path, cb) {
  let log;
  try { log = abc.mmap("ULOG", path, "r"); } catch (e) { return; }
  log.buffer.watermark = log.byteLength;     // map is full; expose all rows
  log.rewind();
  while (log.next()) cb(log);
}

function drain(path) {
  const rows = [];
  each(path, function (log) {
    rows.push({ ts: log.time, ron: ron.encode(log.time),
                verb: log.verb, uri: new URI(log.uri) });
  });
  return rows;
}

//  --- WRITERS ------------------------------------------------------------

//  CLOCKBAD: the system clock is grossly (> 30 s) behind the ULOG tail — an
//  NTP step / DST / suspend-resume, not the per-call self-bump.  Mirrors
//  sniff/AT.c::SNIFFCheckClock.  10-char ron60 code, thrown as a JS error.
const CLOCKBAD = "CLOCKBAD";
const SKEW_MS_MAX = 30000;

//  Decode a ron60 (BigInt) to absolute ms for skew math.  Layout (abc/RON.c
//  RONToTime): 10 RON64 6-bit digits, MS→LS = YY M DD hh mm ss lll.  We use
//  Date.UTC so the (cancelling) tz/DST offset doesn't enter a DELTA.
function ronToMs(r) {
  r = BigInt(r);
  const d = (k) => Number((r >> BigInt(k * 6)) & 63n);
  const yy = d(9) * 10 + d(8);
  const mon = d(7), day = d(6) * 10 + d(5);
  const hh = d(4), mm = d(3), ss = d(2);
  const ms = d(1) * 64 + d(0);
  return Date.UTC(2000 + yy, mon - 1, day, hh, mm, ss, ms);
}

//  SNIFFAtNow port: a fresh stamp strictly greater than `tail`.  RONNow()
//  is the wall clock; bump to tail+1 when it has not advanced past the tail
//  (a burst within one ms, or a future-stamped tail).  A gross-backwards
//  wall clock (> 30 s behind tail) is a clock fault → CLOCKBAD.
function nowAfter(tail) {
  let now = ron.now();
  if (tail != null && tail > 0n) {
    if (now < tail && (ronToMs(tail) - ronToMs(now)) > SKEW_MS_MAX)
      throw CLOCKBAD + ": system clock is before the latest wtlog row";
    if (now <= tail) now = tail + 1n;
  }
  return now;
}

//  Build the DATA region of a fresh ULOG over `rows` in RAM.  Each row's
//  explicit ts is fed verbatim; the container's own monotonic guard keeps
//  same-ms rows strictly increasing.  Returns a Uint8Array (its own copy).
function buildUlog(rows) {
  const log = abc.ram("ULOG", Math.max(1 << 16, rows.length * 256));
  for (const r of rows) log.feed(r.verb, r.uri, r.ts);
  const n = Number(log.buffer.watermark);
  return log.subarray(0, n).slice();
}

//  Crash-safe stage: write the bytes to a temp sibling of `path` and return
//  the temp path.  The caller commits with io.rename (atomic within a FS);
//  a crash before that leaves the OLD `path` byte-intact (no resize-in-place).
function _stage(path, rows) {
  const bytes = buildUlog(rows);
  const tmp = path + ".tmp." + (ron.now()).toString(36) +
              "." + (Math.random() * 1e9 | 0);
  const fd = io.open(tmp, "c");
  try {
    const b = io.buf(bytes.length + 8);
    b.feed(bytes);
    io.writeAll(fd, b);
    io.sync(fd);
  } finally { io.close(fd); }
  return tmp;
}

//  Write a fresh ULOG from `rows` over `path`, crash-safely.
function write(path, rows) {
  const tmp = _stage(path, rows);
  try { io.rename(tmp, path); }
  catch (e) { try { io.unlink(tmp); } catch (e2) {} throw e; }
}

//  Append `rows` to the EXISTING ULOG at `bePath`: drain the old rows
//  (preserving their original ts), sample a monotonic new ts strictly past
//  the tail, assign consecutive increasing ts to the new rows, then rewrite.
function append(bePath, rows) {
  const old = [];
  each(bePath, function (log) {
    old.push({ verb: log.verb, uri: log.uri, ts: log.time });
  });
  const tail = old.length ? old[old.length - 1].ts : 0n;
  let ts = nowAfter(tail);
  const fresh = rows.map(function (r) {
    const row = { verb: r.verb, uri: r.uri, ts: (r.ts != null ? BigInt(r.ts) : ts) };
    ts = (row.ts >= ts ? row.ts : ts) + 1n;     // next row strictly later
    return row;
  });
  write(bePath, old.concat(fresh));
}

module.exports = { each: each, drain: drain,
                   write: write, append: append, _stage: _stage,
                   nowAfter: nowAfter, buildUlog: buildUlog,
                   ronToMs: ronToMs, CLOCKBAD: CLOCKBAD };
