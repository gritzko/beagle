"use strict";
// File-backed containers: a heap whose backing IS a mmapped file.  Same verbs,
// the sift writes through to the mapped pages; abc.close msyncs + drops the pin.
function fail(m) { throw "FAIL " + m; }
function eq(a, b, m) { if (a !== b) fail(m + ": " + a + " !== " + b); }

// pair lane (kv64) over a created+mapped file
const h = abc.mmap("HEAPkv64", "/tmp/jabc_mmap_heap.bin", "c", 64);
for (const k of [5, 2, 8, 1]) h.push(BigInt(k), BigInt(k * 10));
eq(h.size, 4, "mmap size");
eq(Number(h.peek()[0]), 1, "mmap peek=min");
let prev = -1;
while (h.size) { const k = Number(h.pop()[0]); if (k < prev) fail("mmap order"); prev = k; }
abc.close(h);

// scalar lane (u64) over its own file
const u = abc.mmap("HEAPu64", "/tmp/jabc_mmap_u64.bin", "c", 16);
for (const k of [9, 4, 6]) u.push(BigInt(k));
eq(Number(u.pop()), 4, "mmap u64 min");
abc.close(u);

// a hash over a mmapped (created, zeroed) file
const m = abc.mmap("HASHu32", "/tmp/jabc_mmap_hash.bin", "c", 256);
m.put(42); m.put(7);
if (!m.has(42) || !m.has(7)) fail("mmap hash put");
if (m.has(5)) fail("mmap hash false positive");
abc.close(m);

io.log("mmap.js OK");
