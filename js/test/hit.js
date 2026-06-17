"use strict";
// HIT bulk ops: sort() a container's live region, then merge / intersect
// sorted runs.  Covers the marshal shapes: scalar bigint (u64), scalar number
// (u32), pair (kv64, by key), blob (sha1, by memcmp).
function fail(m) { throw "FAIL " + m; }
function eq(a, b, m) { if (a !== b) fail(m + ": " + a + " !== " + b); }

// build a heap, push values, sort() -> ascending run
function run(lane, vals, big) {
  const h = abc.ram("HEAP" + lane, 64);
  for (const v of vals) h.push(big ? BigInt(v) : v);
  h.sort();
  return h;
}
const nums = (ta) => Array.from(ta, (x) => Number(x)).join(",");

// --- sort ---
eq(nums(run("u64", [5, 2, 8, 1, 3], true).subarray(0, 5)), "1,2,3,5,8", "u64 sort");
eq(nums(run("u32", [9, 1, 5, 1], false).subarray(0, 4)), "1,1,5,9", "u32 sort");

// --- merge (sorted, deduplicated union) ---
{
  const a = run("u64", [1, 3, 5, 7], true);
  const b = run("u64", [2, 3, 6], true);
  eq(nums(abc.merge([a, b])), "1,2,3,5,6,7", "u64 merge");
}

// --- intersect (values in ALL runs) ---
{
  const a = run("u64", [1, 2, 3, 4, 5], true);
  const b = run("u64", [2, 4, 6], true);
  const c = run("u64", [2, 4, 8], true);
  eq(nums(abc.intersect([a, b, c])), "2,4", "u64 intersect");
}

// --- pair lane: kv64 merge, ordered/deduped by key ---
{
  const a = abc.ram("HEAPkv64", 64);
  a.push(1n, 10n); a.push(5n, 50n); a.sort();
  const b = abc.ram("HEAPkv64", 64);
  b.push(3n, 30n); b.push(7n, 70n); b.sort();
  const m = abc.merge([a, b]);           // BigUint64Array of (key,val) pairs
  const keys = [];
  for (let i = 0; i < m.length; i += 2) keys.push(Number(m[i]));
  eq(keys.join(","), "1,3,5,7", "kv64 merge keys");
}

// --- blob lane: sha1 sort by memcmp (first byte distinguishes) ---
{
  const blob = (b) => { const u = new Uint8Array(20); u[0] = b; return u; };
  const h = abc.ram("HEAPsha1", 64);
  h.push(blob(3)); h.push(blob(1)); h.push(blob(2));
  h.sort();
  eq(h[0 * 20] + "," + h[1 * 20] + "," + h[2 * 20], "1,2,3", "sha1 sort");
}

// --- book a file-backed output, merge into it, close trims to live size ---
{
  const a = run("u64", [1, 3, 5, 7], true);
  const b = run("u64", [2, 3, 6], true);
  const path = "/tmp/jabc_merge.idx";
  const out = abc.book("HEAPu64", path, 64);    // reserve 64 slots (512 bytes)
  abc.merge([a, b], out);                        // write in place, sets out.size
  eq(out.size, 6, "booked merge size");          // 1,2,3,5,6,7 (one 3 deduped)
  eq(nums(out.subarray(0, out.size)), "1,2,3,5,6,7", "booked merge content");
  abc.close(out);                                // msync + ftruncate to 6*8 bytes
  eq(io.stat(path).size, 48, "trimmed file size");   // 6 u64 == 48 bytes, not 512
}

io.log("hit.js OK");
