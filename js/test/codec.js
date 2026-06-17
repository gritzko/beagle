"use strict";
// hex encode/decode/encodeInto + sha1/sha256 (known test vectors).
function fail(m) { throw "FAIL " + m; }
function eq(a, b, m) { if (a !== b) fail(m + ": " + a + " !== " + b); }

// hex roundtrip
{
  const b = new Uint8Array([0xde, 0xad, 0xbe, 0xef]);
  eq(hex.encode(b), "deadbeef", "hex encode");
  const back = hex.decode("deadbeef");
  eq(back.length, 4, "hex decode len");
  eq(hex.encode(back), "deadbeef", "hex roundtrip");
  eq(hex.encode(new Uint8Array(0)), "", "hex empty");
}

// hex.encodeInto (provided buffer)
{
  const dst = new Uint8Array(8);
  const n = hex.encodeInto(new Uint8Array([0xde, 0xad, 0xbe, 0xef]), dst);
  eq(n, 8, "encodeInto n");
  eq(utf8.Decode(dst.subarray(0, n)), "deadbeef", "encodeInto content");
}

// bad hex rejected
{
  let threw = false;
  try { hex.decode("xyz"); } catch (e) { threw = true; }
  if (!threw) fail("bad hex not rejected");
}

// sha1/sha256 known vectors for "abc"
{
  const abc = utf8.Encode("abc");
  const d1 = sha1(abc);
  eq(d1.length, 20, "sha1 len");
  eq(hex.encode(d1), "a9993e364706816aba3e25717850c26c9cd0d89d", "sha1(abc)");
  const d2 = sha256(abc);
  eq(d2.length, 32, "sha256 len");
  eq(hex.encode(d2),
     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
     "sha256(abc)");
  // empty-input vector
  eq(hex.encode(sha1(new Uint8Array(0))),
     "da39a3ee5e6b4b0d3255bfef95601890afd80709", "sha1(empty)");
}

// ron60 codec round-trips (verbs, and a timestamp-width string)
{
  for (const s of ["get", "post", "delete", "26617GKE4b"])
    eq(ron.encode(ron.decode(s)), s, "ron roundtrip " + s);
}

io.log("codec.js OK");
