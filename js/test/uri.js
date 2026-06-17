"use strict";
// URI class over abc/URI: parse into 8 components, compose, escape.
function fail(m) { throw "FAIL " + m; }
function eq(a, b, m) { if (a !== b) fail(m + ": '" + a + "' !== '" + b + "'"); }

// full authority URI
{
  const u = new URI("https://user@host:443/a/b?x=1#frag");
  eq(u.scheme, "https", "scheme");
  eq(u.authority, "//user@host:443", "authority");
  eq(u.user, "user", "user");
  eq(u.host, "host", "host");
  eq(u.port, "443", "port");
  eq(u.path, "/a/b", "path");
  eq(u.query, "x=1", "query");
  eq(u.fragment, "frag", "fragment");
  eq(u.toString(), "https://user@host:443/a/b?x=1#frag", "toString");
}

// beagle-shape URI (no authority): scheme:path?branch#msg
{
  const b = new URI("file:/home/x/.be?/dogs/main#msg");
  eq(b.scheme, "file", "be scheme");
  eq(b.host, "", "be host (none)");
  eq(b.path, "/home/x/.be", "be path");
  eq(b.query, "/dogs/main", "be query");
  eq(b.fragment, "msg", "be fragment");
}

// compose + percent-escape
eq(URI.make("http", "ex.com", "/p", "q=1", "f"), "http:ex.com/p?q=1#f", "make");
eq(URI.unescape(URI.escape("a b/c")), "a b/c", "escape roundtrip");

io.log("uri.js OK");
