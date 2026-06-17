"use strict";
// pol: the poll() event loop.  pol carries READINESS; handlers do their own
// io.* I/O.  Each loop here terminates by draining the queue (untimer / EOF-
// drop); the final stop/throw cases use POLStop, which is sticky until
// pol.init() resets it (and clears the JS handler table).
function fail(m) { throw "FAIL " + m; }
function eq(a, b, m) { if (a !== b) fail(m + ": " + a + " !== " + b); }

// 1) periodic timer fires repeatedly, then self-stops via untimer (queue drains)
let ticks = 0;
let t0 = pol.now();
pol.every(2, (ns) => { if (++ticks >= 3) pol.untimer(); });
pol.run(pol.NEVER);
eq(ticks, 3, "periodic ticks");
if (pol.now() < t0) fail("now() not monotonic");
if (pol.any()) fail("queue not drained after untimer");

// 2) one-shot timer fires exactly once and self-removes
let once = 0;
pol.after(1, () => { once++; });
pol.run(pol.NEVER);
eq(once, 1, "one-shot fires once");
if (pol.any()) fail("one-shot left armed");

// 3) fd readiness: a regular file is always POLLIN-ready; read to EOF -> drop.
//    Exercises POLTrackEvents -> poll() -> JABCPolFd -> router -> handler.
const path = "/tmp/jabc_pol.bin";
const payload = utf8.Encode("hello pol");
let wf = io.open(path, "c");
io.writeAll(wf, payload);
io.resize(wf, payload.length);          // exact size regardless of a stale file
io.close(wf);
let rf = io.open(path, "r");
let b = io.buf(64);
let reads = 0;
pol.watch(rf, pol.IN, (fd, rev) => {
  let n = io.read(fd, b);
  reads++;
  if (n === 0) { io.close(fd); return 0; }   // EOF -> drop the fd
  return pol.IN;                             // keep reading
});
pol.run(pol.NEVER);
eq(utf8.Decode(b.data()), "hello pol", "fd payload");
if (reads < 2) fail("expected a data read then an EOF read");
if (pol.any()) fail("fd not dropped after EOF");

// 4) default handler catches an fd watched without its own handler
let rf2 = io.open(path, "r");
let dflt = 0;
pol.default = (fd, rev) => { dflt++; io.close(fd); return 0; };
pol.watch(rf2, pol.IN);                  // no per-fd handler -> pol.default
pol.run(pol.NEVER);
eq(dflt, 1, "default handler invoked");
pol.default = null;

// 5) a handler throw unwinds out of pol.run
let rf3 = io.open(path, "r");
let threw = false;
pol.watch(rf3, pol.IN, () => { throw "boom"; });
try { pol.run(pol.NEVER); } catch (e) { threw = String(e).indexOf("boom") >= 0; }
if (!threw) fail("handler throw did not propagate");
io.close(rf3);
pol.init();                              // reset the sticky stop flag + table

// 6) pol.stop() breaks the loop even with a live timer
let st = 0;
pol.every(2, () => { if (++st >= 2) pol.stop(); });
pol.run(pol.NEVER);
eq(st, 2, "stop after 2 ticks");
if (!pol.any()) fail("stop should leave the timer armed");
pol.init();                              // clear state before process exit

io.log("pol.js OK");
