#   JABC event loop API (`pol`)

`pol` binds **`abc/POL`** — a thread-local `poll(2)` event loop. POL is a
deadline-ordered heap of pollers; each is either a **file descriptor**
(`tofd > 0`) carrying an interest mask, or the **timer** (`tofd < 0`, encoding
`-period_ms`). Callbacks return the next interest: an fd's next event mask, a
timer's next period.

The tension `pol` resolves: an event loop must remember callbacks, but JABC
principle #4 says *"do not stash the callbacks… only keep the bootstrap
points."* So C holds **no per-fd JS closures**. The `fd → handler` table lives
in JS (the embedded `pol` bundle, exactly as `Buf` lives in `buf.cpp` and the
containers in `cont.cpp`); C holds just **two router refs** — the bootstrap
points — and routes every ready fd / timer tick back through them.

##  Principles

  - **Readiness, not transfer.** `pol` only tells you an fd is ready; the bytes
    move through `io.*` / `Buf`. Same split as `io` (leaf syscalls) vs `Buf`
    (the cursor): `pol` is the leaf poller, the handler does the I/O.
  - **Handlers live in JS.** The `fd → handler` map is a JS `Map` inside the
    `pol` bundle. A single native trampoline routes through one protected JS
    function. Per-fd closures never enter C land.
  - **fd = number.** You `watch`/`unwatch` by fd, like every `io` call. No
    `Watcher` object, no fd→object table.
  - **Return = next interest.** An fd handler returns the next event mask; `0`
    drops the fd. The timer handler returns the next ms; `≥ 1h` removes it.
    These mirror the POL callback contract one-to-one (`POLLoop` ejects an fd
    whose callback returns `0`; `POLTimer` removes a timer asking for `≥ 1h`).
  - **Errors throw out of `run()`.** A handler exception is stashed, the loop is
    stopped (`POLStop`), and `pol.run` re-throws it to its caller.
  - **Re-entrancy is safe.** A handler may `watch`/`unwatch`/`more`/`sooner`/
    `stop` — POL re-resolves heap slots by stable identity after every callback
    (the ABC-001 fix), so reordering/ejecting mid-dispatch is sound.

##  The event model

POL is one deadline-ordered min-heap; the loop wakes at the soonest deadline,
runs ready timers, then `poll()`s the live fds:

```
  heap (by deadline)
   ├─ poller{ tofd =  fd , events }  → JABCPolFd  → pol._fd(fd, revents)  → mask
   ├─ poller{ tofd =  fd , events }  → JABCPolFd  → pol._fd(fd, revents)  → mask
   └─ poller{ tofd = -ms , timer  }  → POLTimer   → pol._timer(deadlineNs)→ ms
```

  - **fd poller** `tofd > 0` — distinguished by the fd number, so one C
    trampoline serves every fd.
  - **timer poller** `tofd < 0` — POL keys timers by the C callback *pointer*,
    so one trampoline == one timer (v1; layer many in JS, see below).

##  Surface

```js
pol.init(maxfd);                 // POLInit — (re)size the fd table (auto: 4096)
pol.any();                       // POLAny — is anything still tracked?
pol.now();                       // POLNow — monotonic ns (Number)

pol.watch(fd, pol.IN, handler);  // POLTrackEvents — interest mask + per-fd handler
pol.watch(lsock, pol.IN);        //   no handler → falls through to pol.default
pol.more(fd, pol.OUT);           // POLAddEvents — OR more bits into the mask
pol.unwatch(fd);                 // POLIgnoreEvents — stop tracking, drop the handler
pol.default = (fd, revents) => { /* catch-all for handler-less fds */ };

pol.every(50, (ns) => {/*tick*/}); // POLTrackTime — periodic timer (period 50ms)
pol.after(200, (ns) => {/*once*/});// one-shot: fires ~200ms out, self-removes
pol.sooner(10);                  // POLAddTime — wake the timer earlier than scheduled
pol.untimer();                   // POLIgnoreTime — cancel the timer

pol.run(pol.NEVER);              // POLLoop(ns): drive until timeout / empty / stop
pol.stop();                      // POLStop — break run() from inside a handler
pol.sleep(ns);                   // POLSleep — plain nanosleep

// interest bits (platform <poll.h> values):  pol.IN OUT ERR HUP PRI NVAL
// time consts (ns):                           pol.SEC pol.MS  pol.NEVER
```

`watch`/`every`/`run`/… are the JS wrappers (the `pol` bundle); the native
leaves they call are `pol._watch` / `_more` / `_unwatch` / `_every` /
`_untimer` / `_run`. You only call the wrappers.

##  Handler contract

```js
// fd handler — return the NEXT interest mask; 0 drops the fd
pol.watch(sock, pol.IN, (fd, revents) => {
  if (revents & (pol.ERR | pol.HUP)) { io.close(fd); return 0; }  // done → drop
  io.read(fd, b);
  return pol.IN;                                                  // keep reading
});

// timer handler — return the NEXT period in ms; ≥ 3600000 (1h) removes it
pol.every(50, (deadlineNs) => { flush(); });   // pol.every fixes the period for you
```

  - **Return value is the interest, not the result.** The handler advances its
    own buffers via `io.*`; what it *returns* is what POL tracks next.
  - **Dropping the fd you handle:** `io.close(fd)` (or `pol.unwatch(fd)`) **and**
    return `0` — otherwise the next `poll()` hits a dead fd as `POLLNVAL`.
  - **A throw unwinds the loop.** The exception surfaces from the `pol.run(...)`
    call; in-flight pollers stay registered (re-enter the loop to resume).

##  Composition

`pol` is the loop; you rarely watch raw fds yourself. The **`net`** module
(see [NET.md]) layers a Node-style async API on top — `net`/`dgram` sockets and
`setTimeout`/`setInterval` are all `pol` registrations under the hood, and the
loop runs implicitly once your script returns:

```js
// echo server — net.* hides pol.watch; the loop drains at exit
const server = net.createServer(sock => {
  sock.on("data", chunk => sock.write(chunk));   // echo (chunk = read-buffer view)
  sock.on("end",  ()    => server.close());
});
server.listen(8080, "127.0.0.1");
setInterval(() => io.log("alive:", pol.any()), 1000);
// no pol.run() — main drives the loop until no fds/timers remain
```

Watching a raw fd directly (the layer `net` is built from) is still available
when you need it — `pol.watch(fd, pol.IN, handler)` with the handler doing its
own `io.read`/`io.write` and returning the next mask (`0` to drop the fd).

##  Lifecycle & ownership

  - C holds exactly **two** protected JS refs: `pol._fd` and `pol._timer` (the
    routers), grabbed once at install. Nothing per-fd, nothing per-call.
  - The per-fd handler is released the instant POL stops tracking the fd: the
    router `delete`s the `Map` entry when a handler returns `0`, on `unwatch`,
    and the timer slot clears on `≥ 1h`.
  - `pol.init` allocates the `pollfd` vector (`POL_MAXFD` entries); call it once
    up front if 4096 fds is wrong. Re-init is refused while a loop is live.

##  Multiple timers (layering note)

POL identifies a timer by its C callback pointer, so the binding exposes **one**
timer trampoline → one timer. Many logical timers compose in JS: keep a small
min-heap of `{dueNs, fn}` in the bundle and have the single `pol._timer` fire
everything now-due and return the delay to the soonest remaining one. Not wired
in v1 — `pol.every`/`pol.after` cover the common case.

##  ABC mapping

| JS                         | ABC (`abc/POL.h`)                         |
|----------------------------|--------------------------------------------|
| `pol.init` / `pol.any`     | `POLInit` (+`POLFree`) / `POLAny`          |
| `pol.now` / `pol.sleep`    | `POLNow` / `POLSleep`                      |
| `pol.watch(fd, ev, h)`     | `POLTrackEvents` (trampoline `JABCPolFd`)  |
| `pol.more` / `pol.unwatch` | `POLAddEvents` / `POLIgnoreEvents`         |
| `pol.every` / `pol.after`  | `POLTrackTime` (trampoline `POLTimer`)     |
| `pol.sooner` / `pol.untimer`| `POLAddTime` / `POLIgnoreTime`            |
| `pol.run(ns)`              | `POLLoop` (`pol.NEVER` → `POLNever`)       |
| `pol.stop`                 | `POLStop`                                  |
| handler `→ mask` / `→ ms`  | `pollcb` `→ short` / `timercb` `→ u32`     |
| `pol.IN OUT ERR HUP …`     | `POLLIN POLLOUT POLLERR POLLHUP …`         |
