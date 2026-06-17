"use strict";
// Offset-pure PACK log + DELT.apply: write a raw object and an OFS_DELTA
// against it, then read it back and resolve the delta by offset — the keeper
// read loop (seek -> inflate -> chase base by offset -> apply) in JS.
function fail(m) { throw "FAIL " + m; }
function eq(a, b, m) { if (a !== b) fail(m + ": " + a + " !== " + b); }
const dec = (u8) => utf8.Decode(u8);

// two near-identical sizable blobs of VARIED text (distinct lines -> the
// delta matcher finds the long common runs, so the delta beats raw)
const lines = [];
for (let i = 0; i < 200; i++) lines.push("line " + i + ": some unique content " + (i * 7));
const s1 = lines.join("\n");
lines[100] = "this line was modified";
const s2 = lines.join("\n");
const v1 = utf8.Encode(s1);
const v2 = utf8.Encode(s2);

const p = abc.ram("PACK", 1 << 16);
p.header();
const a = p.feed("blob", v1);            // raw object  -> offset a
const b = p.feed("blob", v2, a);         // suspected-prev @a -> OFS_DELTA
p.finish();
eq(p.count, 2, "object count");

// raw read-back
p.seek(a);
eq(p.type, "blob", "a type");
eq(p.size, v1.length, "a size");
const o1 = io.buf(v1.length + 16);
p.inflate(o1);
eq(dec(o1.data()), dec(v1), "a inflate == v1");

// v2 came back as a delta against a
p.seek(b);
eq(p.type, "ofs-delta", "b is ofs-delta");
const ba = p.baseOffset;
eq(ba, a, "b baseOffset == a");
const instr = io.buf(v2.length + 64);
p.inflate(instr);                         // the delta instructions

// resolve: chase the base by offset, inflate it, apply the delta
p.seek(ba);
const base = io.buf(v1.length + 16);
p.inflate(base);
const recon = io.buf(v2.length + 16);
delt.apply(base.data(), instr.data(), recon);
eq(dec(recon.data()), dec(v2), "delta resolved == v2");

// sequential walk
p.rewind();
eq(p.next(), true, "next a"); eq(p.offset, a, "walk a");
eq(p.next(), true, "next b"); eq(p.offset, b, "walk b");
eq(p.next(), false, "walk end");

// offset-only: a sha argument is rejected
let threw = false;
try { p.seek(new Uint8Array(20)); } catch (e) { threw = true; }
if (!threw) fail("sha addressing not rejected");

io.log("pack.js OK");
