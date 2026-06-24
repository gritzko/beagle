//  JSQUE-003: the transient job-queue API over a `.be/queue` ULOG.  Plain
//  FIFO consumed WHILE appended: a read cursor walks rows, handlers feed fresh
//  rows at the watermark (fan-out), done when the cursor reaches the tail.
//  File-backed + crash-safe: a clean exit unlinks, so a SURVIVOR at startup is
//  an interrupted run to RESUME (re-seed only when fresh).  See JSQUE-001/006.
"use strict";

const ulog = require("shared/ulog.js");   // JSQUE-016: lib/ -> shared/

//  Generous headroom so the held container never re-books mid-run; rows fed
//  during a run land in the booked IDLE (no SIGBUS — booked, not "rw").
const QUEUE_ROWS = 1 << 16;

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

  const o = ulog._book(path, QUEUE_ROWS);    // book-or-resume the survivors
  const c = o.c;
  let tail = o.tail;
  if (fresh && seedRows && seedRows.length) {
    ulog.feedRows(c, seedRows, tail, null);
    tail = c._lastTs;
  }
  c.rewind();
  const startOff = fresh ? 0 : _readDone(donePath);
  c._read = startOff | 0;                    // skip already-consumed rows

  const q = {
    path: path, donePath: donePath, _c: c,
    //  append: enqueue rows at the tail via the held feed head (streaming).
    append: function (rows) {
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
