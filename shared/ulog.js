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

//  DIS-057: step a ron60 by `k` MILLISECONDS.  A raw BigInt `r + k` corrupts the
//  low 12-bit ms field (ms>=1000 → RONToTime rejects → FILESetMtime stamps
//  epoch-0), so the patch stamp band MUST step in ms.  The date does the
//  calendar: decode to absolute UTC ms, add k, read the fields off the Date.
function ronStepMs(r, k) {
  const t = new Date(ronToMs(r) + (k | 0));
  const yy = t.getUTCFullYear() - 2000, mon = t.getUTCMonth() + 1;
  const day = t.getUTCDate(), hh = t.getUTCHours(), mm = t.getUTCMinutes();
  const ss = t.getUTCSeconds(), ms = t.getUTCMilliseconds();
  let o = 0n; const set = (i, v) => { o |= (BigInt(v) & 63n) << BigInt(i * 6); };
  set(9, Math.floor(yy / 10)); set(8, yy % 10); set(7, mon);
  set(6, Math.floor(day / 10)); set(5, day % 10);
  set(4, hh); set(3, mm); set(2, ss);
  set(1, Math.floor(ms / 64)); set(0, ms % 64);
  return o;
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

//  --- NATIVE WRITERS (JS-073) --------------------------------------------
//  write/append now drive the C ULOG family (abc._ulog_open/_append/_close =
//  ULOGOpen/ULOGAppendAt/ULOGClose) instead of the JS drain-and-rewrite.  Two
//  wins: append is a genuine in-place, crash-safe booked append (survivors are
//  never re-fed, so their ts is preserved byte-for-byte — the wtlog/refs row ts
//  MUST equal the file-mtime the verbs stamp, DIS-057/classify band), and the
//  `.<base>.idx` sidecar is maintained the way native `be` expects.  ULOGAppendAt
//  writes rec.ts VERBATIM and refuses ts<=tail (ULOGCLOCK), so callers hand us
//  the exact stamp and we only keep the sequence strictly increasing.

//  The ULOG sidecar path `<dir>/.<base>.idx` (dog/ULOG.c ulog_idx_path).
function idxPath(path) {
  const i = path.lastIndexOf("/");
  return (i < 0) ? "." + path + ".idx"
                 : path.slice(0, i + 1) + "." + path.slice(i + 1) + ".idx";
}

//  Callers pass string URIs; a URI object is stringified defensively.
function _uri(u) { return (typeof u === "string") ? u : String(u); }

//  Write a fresh ULOG from `rows` over `path`, crash-safely: build into a temp
//  sibling via the native writer, then io.rename onto `path` (a kill before the
//  rename leaves the OLD file byte-intact).  Explicit row ts are honoured
//  VERBATIM (put's drained old rows keep their small original ts); a no-ts row
//  samples a fresh monotonic stamp.  The stale sidecars are dropped so the next
//  native open rebuilds one matching the renamed file (ULOGOpenIdx self-heals).
function write(path, rows) {
  const tmp = path + ".tmp." + (ron.now()).toString(36) +
              "." + (Math.random() * 1e9 | 0);
  try { io.unlink(tmp); } catch (e) {}
  try { io.unlink(idxPath(tmp)); } catch (e) {}
  const h = abc._ulog_open(tmp);
  //  PUT-012: return each row's ASSIGNED ts (in order) so a restamping caller
  //  (submount.mount) stamps files to the exact track-row stamp, never re-parses.
  const assigned = [];
  try {
    let ts = 0n;                              // 0 = no row yet (honour small ts)
    for (const r of rows) {
      let use = (r.ts != null) ? BigInt(r.ts) : (ts > 0n ? ts : nowAfter(0n));
      if (use <= ts) use = ts + 1n;           // strictly increasing (native guard)
      abc._ulog_append(h, use, r.verb, _uri(r.uri));
      assigned.push(use);
      ts = use;
    }
  } catch (e) {
    try { abc._ulog_close(h); } catch (e2) {}
    try { io.unlink(tmp); } catch (e2) {}
    try { io.unlink(idxPath(tmp)); } catch (e2) {}
    throw e;
  }
  abc._ulog_close(h);                         // trims tmp to PAST+DATA + sidecar
  io.rename(tmp, path);
  try { io.unlink(idxPath(tmp)); } catch (e) {}
  try { io.unlink(idxPath(path)); } catch (e) {}
  return assigned;
}

//  Append `rows` to the ULOG at `path` IN PLACE (native booked append): open,
//  stamp each new row strictly past the live tail (nowAfter(tail) then +1 per
//  row).  POST-029: an explicit row ts is honoured whenever it stays STRICTLY
//  past the running floor (all the native ULOGAppendAt refuses is ts<=last) —
//  the old `use < nowAfter` guard silently re-stamped every ms-stale explicit
//  ts, so a verb's row-ts mtime restamp matched no row.  Only a COLLIDING row
//  bumps.  Returns the ASSIGNED ts (BigInt) per row, in order — the stamp each
//  row REALLY got; a restamping caller must use it.  Old rows are NOT re-fed —
//  they keep their original ts and bytes.  Creates the file if absent.
function append(path, rows) {
  const h = abc._ulog_open(path);
  const assigned = [];
  try {
    const n = abc._ulog_count(h);
    const tail = n > 0 ? abc._ulog_rowTime(h, n - 1) : 0n;
    let last = tail;
    let ts = nowAfter(tail);
    for (const r of rows) {
      let use = (r.ts != null) ? BigInt(r.ts) : ts;
      if (use <= last) use = ts;              // collision only (native refuses ts<=last)
      abc._ulog_append(h, use, r.verb, _uri(r.uri));
      assigned.push(use);
      last = use;
      ts = use + 1n;
    }
  } finally { abc._ulog_close(h); }
  return assigned;
}

//  --- STREAMING TAIL-APPEND (JSQUE-003) ----------------------------------

//  Per-row IDLE headroom for a booked sparse file (verb + 2KB URI + ts + nl).
const ROW_CAP = 2048;

//  Read the existing ULOG bytes (empty if absent), then open a BOOKED sparse
//  "c" file (abc.book truncates on open, so snapshot first) sized to the old
//  bytes + `growRows` of headroom, copy the survivors back in, and position
//  the feed head past them.  Returns { c, tail } — ONE container whose read
//  cursor and feed head are the same instance (JSQUE-003).  NOT an "rw" map:
//  an "rw" map is exact-length, zero IDLE, and SIGBUSes when fed past .end.
function _book(path, growRows) {
  let old = new Uint8Array(0);
  try { old = abc.mmap("ULOG", path, "r").slice(); } catch (e) {}
  const cap = old.length + Math.max(1, growRows | 0) * ROW_CAP;
  const c = abc.book("ULOG", path, cap);     // sparse file, all IDLE, wm 0
  if (old.length) {
    c.set(old, 0);                           // restore survivors into DATA
    c.buffer.watermark = old.length;
  }
  //  seed monotonic guard + tail from the survivors' last row.
  let tail = 0n;
  if (old.length) { c.rewind(); while (c.next()) tail = c.time; c._lastTs = tail; }
  return { c: c, tail: tail };
}

//  Trim the booked file to its live write head and drop the mapping pin.
function _trim(c) { abc.close(c); }

//  JSQUE-020: appendInPlace + feedRows retired with core/job.js (their sole
//  callers).  _book/_trim stay as the crash-safe booked-file primitives.

//  --- REVERSE SEEK WRAPPERS (JSQUE-003; JSQUE-020 dropped seekBack) --------

//  Expose a RO ULOG with its whole length visible, run `fn(log)`, close.  The
//  cursor is held only inside the frame (rule #4).
function _withRO(path, fn) {
  let log;
  try { log = abc.mmap("ULOG", path, "r"); } catch (e) { return undefined; }
  log.buffer.watermark = log.byteLength;
  return fn(log);
}

//  Read the row currently under a positioned cursor into a plain object.
function _row(log) {
  return { offset: log.offset, ts: log.time, verb: log.verb, uri: log.uri };
}

//  prevRow(path, offset): the row immediately preceding `offset` (its start is
//  before `offset`), or undefined at the head.  A forward next() from 0 stops
//  at the last row that starts strictly before `offset`.
function prevRow(path, offset) {
  return _withRO(path, function (log) {
    if (offset == null || offset <= 0) return undefined;
    let prev;
    log.rewind();
    while (log.next() && log.offset < offset) prev = _row(log);
    return prev;
  });
}

module.exports = { each: each, drain: drain,
                   write: write, append: append, _stage: _stage,
                   _book: _book, _trim: _trim,
                   prevRow: prevRow,
                   nowAfter: nowAfter, buildUlog: buildUlog,
                   ronToMs: ronToMs, ronStepMs: ronStepMs, CLOCKBAD: CLOCKBAD };
