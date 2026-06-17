"use strict";
// net + timers over pol: a Node-style async API.  Three scenarios run
// concurrently in the one implicit event loop (main drains pol after the
// script returns); each sets its done flag, and the last one prints OK.  A
// thrown assertion in any callback propagates out of pol.run -> non-zero exit.
function fail(m) { throw "FAIL " + m; }
function eq(a, b, m) { if (a !== b) fail(m + ": " + a + " !== " + b); }

const done = { echo: false, big: false, timers: false, udp: false };
function maybeDone() { if (done.echo && done.big && done.timers && done.udp) io.log("net.js OK"); }

// 1) TCP echo round-trip: connect, write+end, read the echo back, close
(function () {
  let got = "";
  const srv = net.createServer(sock => {
    sock.on("data", chunk => sock.write(chunk));   // echo
    sock.on("end", () => srv.close());
  });
  srv.listen(19200, "127.0.0.1", () => {
    const c = net.connect(19200, "127.0.0.1", () => { c.write("hello net"); c.end(); });
    c.on("data", chunk => { got += utf8.Decode(chunk); });
    c.on("close", () => { eq(got, "hello net", "echo payload"); done.echo = true; maybeDone(); });
  });
})();

// 2) large payload: > the 64K read buffer, so many chunks + a grown write FIFO;
//    verify every byte and the exact total round-trips intact
(function () {
  const N = 200000;
  const srv = net.createServer(sock => {
    sock.on("data", chunk => sock.write(chunk));
    sock.on("end", () => srv.close());
  });
  srv.listen(19201, "127.0.0.1", () => {
    const sent = new Uint8Array(N);
    for (let i = 0; i < N; i++) sent[i] = i & 0xff;
    let rxLen = 0, exp = 0, ok = true;
    const c = net.connect(19201, "127.0.0.1");
    c.on("data", chunk => {
      for (let i = 0; i < chunk.length; i++) if (chunk[i] !== ((exp++) & 0xff)) ok = false;
      rxLen += chunk.length;
    });
    c.on("close", () => { if (!ok) fail("big payload bytes"); eq(rxLen, N, "big payload len"); done.big = true; maybeDone(); });
    c.write(sent); c.end();
  });
})();

// 3) UDP (dgram): ping/pong round-trip, sender rinfo echoed back to
(function () {
  const srv = dgram.createSocket("udp4", (msg, rinfo) => {
    eq(utf8.Decode(msg), "udp-ping", "udp server msg");
    srv.send("udp-pong", rinfo.port, rinfo.address);
  });
  srv.bind(19202, "127.0.0.1", () => {
    const cli = dgram.createSocket("udp4", (msg) => {
      eq(utf8.Decode(msg), "udp-pong", "udp client reply");
      cli.close(); srv.close(); done.udp = true; maybeDone();
    });
    cli.bind(0, "127.0.0.1", () => cli.send("udp-ping", 19202, "127.0.0.1"));
  });
})();

// 4) timers: setTimeout fires, clearTimeout cancels, setInterval fires exactly
//    3 times then clearInterval stops it
(function () {
  const log = [];
  setTimeout(() => log.push("a"), 5);
  const cancel = setTimeout(() => log.push("CANCELLED"), 5);
  clearTimeout(cancel);
  let n = 0;
  const iv = setInterval(() => {
    if (++n === 3) {
      clearInterval(iv);
      if (log.indexOf("CANCELLED") >= 0) fail("clearTimeout did not cancel");
      if (log.indexOf("a") < 0) fail("setTimeout did not fire");
      eq(n, 3, "interval count");
      done.timers = true; maybeDone();
    }
  }, 5);
})();
