"use strict";
// Blob lanes (sha1=20B, sha256=32B) as Uint8Array, for both HEAP and HASH.
// HEAP: push out of order, pop ascending by memcmp; peek = min; size; drain.
// HASH: put/has/get (returns stored blob) back; miss absent; del only one.
function fail(m) { throw "FAIL " + m; }
function eq(a, b, m) { if (a !== b) fail(m + ": " + a + " !== " + b); }

// A distinct blob of N bytes that sorts by memcmp on its first byte.
function blob(N, v) { const b = new Uint8Array(N); b[0] = v; return b; }
function beq(a, b) {
  if (!a || !b || a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

const lanes = { sha1: 20, sha256: 32 };

for (const L in lanes) {
  const N = lanes[L];

  // --- HEAP ---
  const h = abc.ram("HEAP" + L, 64);
  eq(h.size, 0, L + " heap empty");
  const order = [5, 2, 8, 1, 3, 7];
  for (const v of order) h.push(blob(N, v));
  eq(h.size, order.length, L + " heap size");
  if (!beq(h.peek(), blob(N, 1))) fail(L + " heap peek=min");

  let prev = -1;
  for (let i = 0; i < order.length; i++) {
    const r = h.pop();
    if (r.length !== N) fail(L + " heap pop length " + r.length);
    const key = r[0];
    if (key < prev) fail(L + " heap not ascending: " + key + " after " + prev);
    prev = key;
  }
  eq(h.size, 0, L + " heap drained");
  eq(h.pop(), undefined, L + " heap pop empty");

  // --- HASH ---
  const t = abc.ram("HASH" + L, 256);   // power of 2, zeroed
  const keys = [11, 47, 99, 200];
  for (const v of keys) t.put(blob(N, v));
  for (const v of keys) {
    if (!t.has(blob(N, v))) fail(L + " hash missing " + v);
    const g = t.get(blob(N, v));
    if (!beq(g, blob(N, v))) fail(L + " hash get mismatch " + v);
  }
  if (t.has(blob(N, 250))) fail(L + " hash false positive");
  t.del(blob(N, keys[0]));
  if (t.has(blob(N, keys[0]))) fail(L + " hash not deleted " + keys[0]);
  for (let i = 1; i < keys.length; i++)
    if (!t.has(blob(N, keys[i]))) fail(L + " hash del clobbered " + keys[i]);
}
io.log("sha.js OK");
