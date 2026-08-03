//  memidx.js — DOG-027: the in-RAM (key,val) side-index that the retired
//  `abc.index("wh128", {mem: N})` mode used to serve.  `abc.index` is now a
//  HANDLE on an on-disk dog Pup stack and has no RAM-only mode, but three
//  callers want a transient index with NO files at all: store.js's scan-build
//  fallback (a read-only store must never write), graf.js's pair memtable and
//  head.js's remote-DAG overlay over a fetched pack.
//
//  One sorted `abc.ram("HEAPwh128")` heap behind put + range — the same abc
//  leaves the old JS memtable rode (`sort` + `_seekrange_wh128`), no LSM, no
//  ladder, no naming.  `slots` is the hard capacity (push past it throws).
//  Rows are sorted LAZILY, on the first range() after a put.

"use strict";

function open(slots) {
  const ram = abc.ram("HEAPwh128", slots);
  let sorted = true;

  return {
    //  put(key, val): append; the sort is deferred to the next range().
    put: function (k, v) { ram.push(BigInt(k), BigInt(v)); sorted = false; return this; },

    //  range(lo, hi, cb): ordered [lo, hi) scan, cb(kv) in-frame; a `false` /
    //  "enough" return stops it (the abc.index range contract).
    range: function (lo, hi, cb) {
      const n = ram.buffer.watermark | 0;
      if (!n) return this;
      if (!sorted) { ram.sort(); sorted = true; }
      abc._seekrange_wh128([ram.subarray(0, n * 2)],
                           BigInt(lo), 0n, BigInt(hi), 0n, cb);
      return this;
    },

    //  live entry count (the `mem` mode's only other reader).
    get count() { return ram.buffer.watermark | 0; }
  };
}

module.exports = { open: open };
