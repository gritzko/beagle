//  JSQUE-003: the transient job-queue API over a `.be/queue` ULOG.  Plain
//  FIFO consumed WHILE appended: a read cursor walks rows, handlers feed fresh
//  rows at the watermark (fan-out), done when the cursor reaches the tail.
//  File-backed + crash-safe: a clean exit unlinks, so a SURVIVOR at startup is
//  an interrupted run to RESUME (re-seed only when fresh).  See JSQUE-001/006.
"use strict";

const ulog = require("shared/ulog.js");   // JSQUE-016: lib/ -> shared/

//  JOBQ: a queue normally holds a handful of rows, so book SMALL and GROW on
//  demand — the old `1 << 16` rows × ROW_CAP pre-allocated ~128 MB per run for
//  a 2-row queue.  SEED_ROWS books a few hundred rows (~half a MB of IDLE);
//  appendInPlace's grow-on-demand (below) doubles the booked file whenever a
//  feed would near the booked end, so a big fan-out still works without the
//  giant up-front alloc.  (ROW_CAP is ulog.js's per-row IDLE estimate, ~2 KB.)
const SEED_ROWS = 256;
const ROW_CAP = 2048;                  // mirrors shared/ulog.js ROW_CAP
//  Re-book when the live write head is within this many bytes of the booked
//  end — one ROW_CAP of slack so a single feed can never run off the cap.
const GROW_SLACK = ROW_CAP;

//  Read the persisted consumed offset (the `.done` side-row), 0 if absent or
//  unparsable — handlers are idempotent, so falling back to 0 re-replays.
function _readDone(donePath) {
  let off = 0;
  ulog.each(donePath, function (log) {
    const n = parseInt(log.uri, 10);
    if (!isNaN(n) && n >= 0) off = n;
  });
  return off;
}

//  openOrResume(path, seedRows): a SURVIVING file resumes (its `.done` offset
//  skips consumed rows); a fresh/empty file is seeded with `seedRows`.  Holds
//  ONE booked container open for the whole run — the read cursor and the feed
//  head are the same instance (JSQUE-003).
function openOrResume(path, seedRows) {
  const donePath = path + ".done";
  let fresh = true;
  try { fresh = io.stat(path).size === 0; } catch (e) { fresh = true; }

  //  Book the survivors over a SMALL initial cap (SEED_ROWS) — grow-on-demand
  //  (below) takes over for a big fan-out, so no giant up-front alloc.
  let c = ulog._book(path, SEED_ROWS).c;
  let tail = c._lastTs || 0n;

  //  Grow-on-demand: re-book the file at ~2× the current capacity, preserving
  //  the live DATA, the write head (watermark), the read cursor (_read), and
  //  the monotonic guard (_lastTs).  abc.book truncates on open, so snapshot
  //  the DATA first.  Returns the new (larger) container; the caller rebinds.
  function _grow(need) {
    const wm = Number(c.buffer.watermark);
    const cap = Number(c.byteLength);
    let next = cap * 2;
    while (next < wm + need + GROW_SLACK) next *= 2;
    const snap = c.slice(0, wm);             // live DATA (own copy)
    const savedRead = c._read | 0;
    const savedTs = c._lastTs;
    ulog._trim(c);                           // drop the old (smaller) mapping
    const c2 = abc.book("ULOG", path, next); // re-book larger (truncates → restore)
    c2.set(snap, 0);
    c2.buffer.watermark = wm;
    c2._read = savedRead;
    c2._lastTs = savedTs;
    c = c2;
  }

  //  Headroom check: a feed of `n` rows needs ~n×ROW_CAP bytes; grow before the
  //  write head reaches the booked end so c.feed never throws "feed (full?)".
  function _ensure(n) {
    const need = (n | 0) * ROW_CAP;
    if (Number(c.buffer.watermark) + need + GROW_SLACK > Number(c.byteLength))
      _grow(need);
  }

  //  Seed a fresh queue (a survivor resumes its own rows — never re-seed).  The
  //  seed batch can exceed SEED_ROWS (a wide arg list), so reserve headroom too.
  if (fresh && seedRows && seedRows.length) {
    _ensure(seedRows.length);
    ulog.feedRows(c, seedRows, tail, null);
    tail = c._lastTs;
  }
  c.rewind();
  const startOff = fresh ? 0 : _readDone(donePath);
  c._read = startOff | 0;                    // skip already-consumed rows

  const q = {
    path: path, donePath: donePath,
    get _c() { return c; },                  // live container (rebound on grow)
    //  append: enqueue rows at the tail via the held feed head (streaming),
    //  growing the booked file first if the batch would near the booked end.
    append: function (rows) {
      _ensure(rows.length);
      ulog.feedRows(c, rows, c._lastTs || tail, null);
      tail = c._lastTs;
      return this;
    },
    //  next: advance the read cursor ONE row, re-reading the watermark each
    //  step so rows fed during iteration are seen (consume-while-append).
    next: function () {
      if (!c.next()) return undefined;       // reached the live tail
      return { ts: c.time, verb: c.verb, uri: c.uri, offset: c.offset };
    },
    //  markDone: persist the consumed offset (the cursor past the last row)
    //  to the `.be/queue.done` side-row; the boundary is the resume point.
    markDone: function () {
      ulog.write(donePath, [{ verb: "done", uri: String(c.after | 0) }]);
      return this;
    },
    //  close(unlink): trim the booked file down + drop the pin; on a clean
    //  exit (unlink=true) remove the queue + its `.done` so a later open is
    //  FRESH (no survivor to resume).
    close: function (unlink) {
      ulog._trim(c);
      if (unlink) {
        try { io.unlink(path); } catch (e) {}
        try { io.unlink(donePath); } catch (e) {}
      }
      return this;
    },
  };
  return q;
}

module.exports = { openOrResume: openOrResume };
