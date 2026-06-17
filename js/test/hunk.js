"use strict";
// HUNK family: a u8 log of TLV 'H' records. dogenize (tokenize source ->
// hunk), feed (pre-formed hunk), next()/current-field getters, render.
function fail(m) { throw "FAIL " + m; }
function eq(a, b, m) { if (a !== b) fail(m + ": " + a + " !== " + b); }
const dec = (u8) => utf8.Decode(u8);

// dogenize a JS source -> one hunk carrying text + tokens
{
  const h = abc.ram("HUNK", 1 << 16);
  h.dogenize(utf8.Encode("let x = 42;\n"), "js", "app.js");
  let n = 0;
  while (h.next()) {
    n++;
    eq(dec(h.uri), "app.js", "dogenize uri");
    eq(dec(h.text), "let x = 42;\n", "dogenize text");
    if (h.toks.length === 0) fail("dogenize produced no tokens");
  }
  eq(n, 1, "dogenize hunk count");
}

// feed a pre-formed hunk, drain it back (fields are zero-copy views)
{
  const h = abc.ram("HUNK", 1 << 16);
  h.feed("a/b.c", utf8.Encode("hello"), new Uint32Array(0));
  if (!h.next()) fail("feed/next");
  eq(dec(h.uri), "a/b.c", "feed uri");
  eq(dec(h.text), "hello", "feed text");
}

// several hunks in one buffer, walk them all
{
  const h = abc.ram("HUNK", 1 << 16);
  h.feed("one", utf8.Encode("1"), new Uint32Array(0));
  h.feed("two", utf8.Encode("2"), new Uint32Array(0));
  h.feed("three", utf8.Encode("3"), new Uint32Array(0));
  const uris = [];
  while (h.next()) uris.push(dec(h.uri));
  eq(uris.join(","), "one,two,three", "multi-hunk walk");
  h.rewind();                          // read cursor back to the start
  if (!h.next()) fail("rewind");
  eq(dec(h.uri), "one", "rewind uri");
}

// render the current hunk to plain text into an out buffer
{
  const h = abc.ram("HUNK", 1 << 16);
  h.dogenize(utf8.Encode("y\n"), "js", "f.js");
  h.next();
  const out = io.buf(1 << 12);
  h.plain(out);
  if (dec(out.data()).indexOf("f.js") < 0) fail("plain render missing uri");
}

io.log("hunk.js OK");
