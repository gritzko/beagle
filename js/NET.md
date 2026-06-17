#   JABC net / dgram / timers API

A **Node-style async API** — `net` (TCP), `dgram` (UDP), and the global
`setTimeout`/`setInterval` timers — layered on the [`pol`][POL.md] event loop.
The native side is leaf-only: socket syscalls that return bare fds (like
`io.*`), with EAGAIN surfaced as `-1`. Everything async — the EventEmitter
dispatch, the per-socket read/write buffers, and the timer wheel — lives in the
embedded JS bundle. A socket is an fd registered with `pol.watch`; readiness
drives the events. Nothing in C holds memory or a JS closure.

##  Principles

  - **Names mirror Node.** `net.createServer`/`connect`, `server.listen`, socket
    `.on('data'|'end'|'error'|'drain'|'connect'|'close')` / `.write` / `.end`,
    `dgram.createSocket`, `setTimeout`/`setInterval`. If you know Node, you know this.
  - **The loop is implicit.** Once your top-level script returns, `main` drives
    `pol` until no fds/timers remain (Node's model). You never call `pol.run()`.
  - **`data` is a zero-copy view, not a private `Buffer`.** The chunk is a
    `Uint8Array` over the socket's reusable read buffer, valid for the callback's
    duration. `chunk.slice()` to retain. (See [POL.md] "Composition".)
  - **Errors throw out of the loop.** A failed syscall (other than EAGAIN) maps
    `errno`→a JS exception; an uncaught one unwinds the implicit `pol.run`.

##  TCP server

```js
const server = net.createServer(sock => {     // 'connection' shorthand
  sock.on("data",  chunk => sock.write(chunk));   // echo; chunk = view (slice to keep)
  sock.on("end",   ()    => sock.end());          // peer FIN
  sock.on("error", e     => io.log("conn", e));
  sock.on("close", ()    => {});
});
server.listen(8080, "127.0.0.1", () => io.log("up"));  // host defaults to 0.0.0.0
server.on("connection", sock => {});                   // or the createServer arg
server.close();                                        // stop accepting
```

##  TCP client

```js
const c = net.connect(8080, "127.0.0.1", () => c.write("hello\n"));  // 'connect' cb
c.on("data", chunk => io.write(io.stdout, chunk));
c.on("end",  ()    => io.log("peer closed"));
c.write(dataOrString);   // → false above the high-water mark (backpressure hint)
c.end("bye");            // flush, then half-close (FIN); keeps reading until peer FIN
c.destroy();             // drop both sides now
```

`write` takes a string (utf8-encoded), a `Uint8Array`, or a `Buf`. Address form
is a `tcp://host:port` URI built from `(port, host)` and parsed by `abc/NET`.

##  UDP (dgram)

```js
const u = dgram.createSocket("udp4", (msg, rinfo) => {  // 'message' shorthand
  // msg = view over the read buffer; rinfo = { address, port } of the sender
  u.send("pong", rinfo.port, rinfo.address);
});
u.bind(9000, "127.0.0.1", () => io.log("bound"));   // addr defaults to 0.0.0.0
u.send(dataOrString, port, host);                   // host defaults to 127.0.0.1
u.close();
```

`send` resolves `host:port` per call (getaddrinfo, `SOCK_DGRAM`); `'message'`
carries the sender `rinfo` so a reply is `send(.., rinfo.port, rinfo.address)`.

##  Timers (global, Node-identical)

```js
const t = setTimeout(fn, ms, ...args);   clearTimeout(t);
const i = setInterval(fn, ms, ...args);  clearInterval(i);
setImmediate(fn, ...args);               // ≈ setTimeout(fn, 0)
```

All timers are a JS min-heap over `pol`'s single timer (`pol.timer`): each tick
fires everything now-due and re-arms `pol` for the soonest remaining one — so
the one-POL-timer limit is invisible. An active `setInterval` keeps the loop
alive (clear it to let the process exit), exactly like Node.

##  v1 limitations

  - **`connect` is synchronous** under the hood (abc's `TCPConnect` blocks, then
    the fd is flipped nonblocking); the `'connect'` event still fires
    asynchronously on the next tick. Fine for loopback / fast peers.
  - **Backpressure is a hint:** `write()` returns `false` past a high-water mark
    and `'drain'` fires when the buffer empties, but it is not full flow-control.
  - **No `allowHalfOpen`:** a read EOF auto-ends the write side.
  - **`dgram` is IPv4-first** (`getaddrinfo` `AF_UNSPEC`, first result wins).

##  ABC mapping

| JS                              | ABC / syscall                              |
|---------------------------------|--------------------------------------------|
| `net.createServer().listen`     | `TCPListen` (`tcp://host:port`) + nonblock |
| `net.connect`                   | `TCPConnect` + nonblock                    |
| accept loop                     | `TCPAccept` (EAGAIN → `-1`)                |
| socket read / write             | `recv` / `send(MSG_NOSIGNAL)` (EAGAIN → `-1`) |
| `socket.end`                    | `shutdown(SHUT_WR)` after flush            |
| `socket.destroy` / `close`      | `close(2)`                                 |
| `dgram.bind`                    | `UDPBind` + nonblock                       |
| `dgram` recv / send             | `recvfrom` + `getnameinfo` / `getaddrinfo` + `sendto` |
| every socket fd                 | a `pol.watch` registration                 |
| `setTimeout` / `setInterval`    | a JS wheel over `pol.timer`                |
| the implicit loop               | `pol.run(pol.NEVER)` after the script      |
