"use strict";
// ULOG: append-only (ts,verb,uri) log, no index. feed + sequential walk +
// the six offset-pure seek* (forward and reverse).
function fail(m) { throw "FAIL " + m; }
function eq(a, b, m) { if (a !== b) fail(m + ": " + a + " !== " + b); }

const log = abc.ram("ULOG", 1 << 16);
log.feed("post", "?/dogs/main#a");
log.feed("get", "//host/repo");
log.feed("post", "?/dogs/main#b");

// sequential walk + monotonic timestamps
log.rewind();
const vs = [], ts = [];
while (log.next()) { vs.push(log.verb); ts.push(log.time); }
eq(vs.join(","), "post,get,post", "walk verbs");
if (!(ts[0] < ts[1] && ts[1] < ts[2])) fail("monotonic ts");

// current fields by offset
log.rewind(); log.next();
eq(log.uri, "?/dogs/main#a", "uri row0");

// seekVerb forward, chained via .after
let o = log.seekVerb(0, "post");
if (o < 0) fail("seekVerb"); eq(log.uri, "?/dogs/main#a", "1st post");
o = log.seekVerb(log.after, "post");
if (o < 0) fail("2nd post"); eq(log.uri, "?/dogs/main#b", "2nd post");
eq(log.seekVerb(log.after, "post"), -1, "no 3rd post");

// seekVerbRev from .end (newest-first), stepped via o-1
o = log.seekVerbRev(log.end, "post"); eq(log.uri, "?/dogs/main#b", "rev 1st post");
o = log.seekVerbRev(o - 1, "post"); eq(log.uri, "?/dogs/main#a", "rev 2nd post");

// seekURI prefix
o = log.seekURI(0, "?/dogs/");
if (o < 0) fail("seekURI"); eq(log.uri, "?/dogs/main#a", "uri prefix hit");
eq(log.seekURI(0, "ftp://"), -1, "uri prefix miss");

// seekTime: first row with ts >= row1's ts (monotonic dimension)
log.rewind(); log.next(); log.next();
const t1 = log.time;                      // row1 ("get //host/repo")
eq(log.seekTime(0, t1) >= 0, true, "seekTime hit");
eq(log.uri, "//host/repo", "seekTime >= t1 -> row1");
o = log.seekTimeRev(log.end, t1);          // newest row with ts <= t1
eq(log.uri, "//host/repo", "seekTimeRev <= t1 -> row1");

// offset-only addressing
let threw = false;
try { log.seek(new Uint8Array(4)); } catch (e) { threw = true; }
if (!threw) fail("sha addressing not rejected");

io.log("ulog.js OK");
