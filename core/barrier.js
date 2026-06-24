//  JSQUE-006: the JOIN primitive over a job-queue ULOG (boundary marker +
//  back-scan fold).  A barrier is a fold-verb row that, when reached,
//  BACK-SCANS via lib/ulog.js::seekBack (JSQUE-003) to its boundary marker
//  and folds the intervening leaf rows into ONE result.  Post-order emission
//  (marker, leaves, then the fold row) gives the ordering; plain FIFO +
//  seekBack do the rest.  Results are DURABLE — the fold re-reads the rows,
//  holds nothing in memory — so a replay over the same range is idempotent
//  (same range -> same aggregate).  Nested barriers chain via rows: a fold's
//  result row is a leaf of an outer fold (blobs -> subtree -> root tree).
//  Mirrors the C wtlog pd/patch boundary semantics (sniff/AT.c; wtlog.js
//  boundaries() + eachPutDelete's strictly-after-the-floor fold).
"use strict";

const ulog = require("shared/ulog.js");   // JSQUE-016: lib/ -> shared/

//  emit(path, markerVerb, markerUri, leafRows, foldVerb, foldUri): append a
//  boundary marker, then the leaf rows, then the fold-verb row — POST-ORDER,
//  so the fold sits after all of its inputs.  One streaming tail-append.
function emit(path, markerVerb, markerUri, leafRows, foldVerb, foldUri) {
  const rows = [{ verb: markerVerb, uri: markerUri }];
  for (const r of (leafRows || [])) rows.push({ verb: r.verb, uri: r.uri });
  rows.push({ verb: foldVerb, uri: foldUri });
  ulog.appendInPlace(path, rows);
  return path;
}

//  fold(path, foldOffset, markerVerb, fn, acc): the fold runner.  BACK-SCAN
//  from the fold row (at foldOffset) to its newest boundary `markerVerb` row
//  (seekBack), then RE-READ the leaves strictly between the marker and the
//  fold row — the (marker, here) range, mirroring eachPutDelete's `> floor`
//  — applying fn(acc, leafRow) over each in order.  Returns
//  { acc, marker, count }.  Durable + idempotent: nothing is held; a replay
//  over the same range yields the same acc.
function fold(path, foldOffset, markerVerb, fn, acc) {
  const marker = ulog.seekBack(path, markerVerb, foldOffset);
  let count = 0;
  ulog.each(path, function (log) {
    //  (marker, here): leaves AFTER the boundary, BEFORE the fold row.
    if (marker && log.offset <= marker.offset) return;
    if (log.offset >= foldOffset) return;
    acc = fn(acc, { offset: log.offset, verb: log.verb, uri: log.uri });
    count++;
  });
  return { acc: acc, marker: marker, count: count };
}

module.exports = { emit: emit, fold: fold };
