"use strict";
// HASH bindings, every lane: put keys, get/has them back, miss an absent key,
// del one and confirm only it is gone.  Pair lanes store/return a val keyed by
// .key; scalar lanes are a set (get returns the value iff present).
function fail(m) { throw "FAIL " + m; }
function eq(a, b, m) { if (a !== b) fail(m + ": " + a + " !== " + b); }

const lanes = ["u8", "u16", "u32", "u64", "kv32", "kv64", "wh64", "wh128"];
const pair = { kv32: 1, kv64: 1 };     // key -> val map
const set = { wh128: 1 };               // set of (key,val) pairs (keyed by both)
const big = { u64: 1, kv64: 1, wh64: 1, wh128: 1 };

for (const L of lanes) {
  const P = !!pair[L], S = !!set[L], B = !!big[L];
  const K = (k) => B ? BigInt(k) : k;
  const h = abc.ram("HASH" + L, 256);   // power of 2, zeroed by anon mmap
  const keys = [1, 7, 42, 100];          // all fit in a u8

  for (const k of keys) {
    if (S || P) h.put(K(k), K(k + 1)); else h.put(K(k));
  }
  for (const k of keys) {
    if (S) {
      if (!h.has(K(k), K(k + 1))) fail(L + " missing pair " + k);
      eq(Number(h.get(K(k), K(k + 1))[1]), k + 1, L + " pair val " + k);
    } else if (P) {
      if (!h.has(K(k))) fail(L + " missing " + k);
      eq(Number(h.get(K(k))), k + 1, L + " val for " + k);
    } else {
      if (!h.has(K(k))) fail(L + " missing " + k);
      eq(Number(h.get(K(k))), k, L + " membership " + k);
    }
  }
  if (S ? h.has(K(99), K(100)) : h.has(K(99))) fail(L + " false positive");
  if (S) h.del(K(keys[0]), K(keys[0] + 1)); else h.del(K(keys[0]));
  if (S ? h.has(K(keys[0]), K(keys[0] + 1)) : h.has(K(keys[0])))
    fail(L + " not deleted");
}
io.log("hash.js OK (" + lanes.length + " lanes)");
