"use strict";
// HEAP bindings, every lane: push out of order, pop must come out ascending
// (min-heap), peek = min, val tracks key for pair lanes, size cursor, empty.
function fail(m) { throw "FAIL " + m; }
function eq(a, b, m) { if (a !== b) fail(m + ": " + a + " !== " + b); }

const lanes = ["u8", "u16", "u32", "u64", "kv32", "kv64", "wh64", "wh128"];
const pair = { kv32: 1, kv64: 1, wh128: 1 };
const big = { u64: 1, kv64: 1, wh64: 1, wh128: 1 };

for (const L of lanes) {
  const P = !!pair[L], B = !!big[L];
  const K = (k) => B ? BigInt(k) : k;
  const h = abc.ram("HEAP" + L, 64);
  eq(h.size, 0, L + " empty");

  const keys = [5, 2, 8, 1, 3, 7];
  for (const k of keys) {
    if (P) h.push(K(k), K(k * 10)); else h.push(K(k));
  }
  eq(h.size, keys.length, L + " size");
  eq(Number(P ? h.peek()[0] : h.peek()), 1, L + " peek=min");

  let prev = -1;
  for (let i = 0; i < keys.length; i++) {
    const r = h.pop();
    const key = Number(P ? r[0] : r);
    if (key < prev) fail(L + " not ascending: " + key + " after " + prev);
    if (P) eq(Number(r[1]), key * 10, L + " val tracks key");
    prev = key;
  }
  eq(h.size, 0, L + " drained");
  eq(h.pop(), undefined, L + " pop empty");
}
io.log("heap.js OK (" + lanes.length + " lanes)");
