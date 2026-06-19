#   js — JABC (JavaScriptCore bindings) module index

JABC is a thin, anti-bloat JavaScriptCore binding: stock `libjavascriptcore`, the binding holds no memory and no long-lived JS references. Two entities cross the boundary — buffers (JS-owned `Uint8Array`s wrapped in a `Buf` cursor) and file descriptors (plain `number`s). The native layer is leaf-only (syscalls + typed-array fills + mmap); the cursor logic lives in JS. Rationale in [README.md], the JS-facing surface and the ABC→JS mapping in [API.md].

##  Native modules

###  JABC.hpp — binding macros + shared decls

`JABC_FN` defines a native callback; `JABC_THROW`/`JABC_UNDEF` are the error/return forms; `JABC_API_OBJECT`/`JABC_API_FN` register a global object / its methods. `JABCBytesOf` is the one boundary touch — a typed array's offset-adjusted backing range as a `u8s` (adds `byteOffset`, NULL-checks a detached buffer).

###  utf8.cpp — text + the bytes helper

 -  `utf8.encodeInto(str, dst)` — encode UTF-8 into a caller-owned `Uint8Array`, return `n`; stops on a code-point boundary (no half multibyte), lone surrogates → U+FFFD.
 -  `utf8.Decode(u8)` — validate (abc `utf8sValid`/`utf8sDrain32`) then build a JS string via `JSStringCreateWithCharacters` (explicit length, embedded NULs survive).
 -  `JSOfCString` / `JABCBytesOf` — small JS string from a C string; typed-array range (defined here, shared).

###  io.cpp — fds + read/write + mmap (no `File` object, no custody table)

 -  `io.open`/`close`/`sync`/`size`/`resize`/`lock`/`unlock`/`stat` — fd lifecycle over abc `FILE*` (`"r"|"rw"|"c"`).
 -  `io._read`/`io._write` — one `read`/`write` of a typed array's bytes, return `n` (0 = EOF); the cursor advance is the JS `Buf`'s job.
 -  `io._mmap` — `FILEMapRO/RW/Create` → `Uint8Array` no-copy, `munmap` on GC (`JABCMapFree`).
 -  `io._ram` — anonymous `MAP_NORESERVE` mmap → `Uint8Array`, `munmap` on GC (`JABCRamFree`).
 -  `io._msync` — flush a mapped typed array's pages (raw `msync`, no descriptor lookup).
 -  `io.log` — write strings / typed arrays to stderr.

###  buf.cpp — the `Buf` cursor class + constructors (embedded JS)

The `Buf` class and the public API are a raw-string-literal JS bundle (no js2c/node). `Buf` wraps a `Uint8Array` + the `PAST|DATA|IDLE` boundaries: `feed`/`feed1`/`feedStr`/`fed` (append), `take`/`skip` (consume), `data`/`idle`/`past` (no-copy views), `shed`/`pop`/`reset`/`shift`/`splice`/`grow`/`msync`. Constructors `io.buf` (heap), `io.ram` (anon mmap), `io.mmap` (file); `io.read`/`write`/`readAll`/`writeAll`/`readv`/`writev` dispatch over `Buf` or a bare `Uint8Array`. `io.book` and native vectored I/O are not wired yet (see Outcome in API/tickets).

###  cont.cpp — container framework over abc logs

Per-(family,lane) JS prototypes bound once to native leaves, plus the mmap constructors (`abc.ram`/`mmap`/`over`/`book`). Families: HEAP/HASH lanes, HUNK, ULOG, PACK. Each native leaf is pure marshalling — resolve a typed array to a `u8s` and call one abc/dog function; no format logic in the binding.

 -  `pack.hpp` — PACK binding (GIT-007), pure marshalling over the dog/git pack core: `_feed`→`PACKu8sFeedObj` (raw|OFS_DELTA decided there), `_resolve`→`PACKResolveOfs`, `_next`→`PACKRecordEnd`; sha→offset index + base resolution stay in JS.

###  pol.cpp — `pol` event loop over abc/POL (one trampoline, JS owns the table)

The `poll(2)` loop binds `abc/POL` keeping JABC rule #4: C holds NO per-fd JS closures. The `fd→handler` table + wrappers live in an embedded JS bundle (like `Buf`); C holds only two protected router refs (`pol._fd`/`pol._timer`) and routes every ready fd / timer tick through them. `pol` carries readiness; handlers do their own `io.*` I/O. v1 = one timer (POL keys timers by C callback pointer). API + contract in [POL.md].

 -  `pol.watch(fd, mask, handler)` / `more` / `unwatch` — per-fd interest + handler; handler returns the next mask (`0` drops the fd). `pol.default` catches handler-less fds.
 -  `pol.every(ms, fn)` / `pol.after(ms, fn)` / `pol.untimer` — periodic / one-shot timer (`fn` returns next ms; `≥1h` removes).
 -  `pol.run(ns)` / `pol.stop` / `pol.sleep` / `pol.any` / `pol.now` — drive (`pol.NEVER`=forever), break, sleep, query; a handler throw unwinds out of `run()`.
 -  `pol.init(maxfd)` — (re)size the fd table + clear state; refused from inside a running loop. Consts: `pol.IN/OUT/ERR/HUP/PRI/NVAL`, `pol.SEC/MS/NEVER`.

###  net.cpp — net/dgram + Node timers over pol (sockets are fds)

Node-style async API on top of `pol`: native socket leaves return bare fds (EAGAIN → `-1`), and the EventEmitter, per-socket read/write `Buf`s, and the `setTimeout`/`setInterval` timer wheel all live in the embedded JS bundle. A socket is an fd registered with `pol.watch`; readiness drives the events; transfers are `recv`/`send`. Full surface + contract in [NET.md].

 -  `net.createServer(onConn)` / `server.listen(port, host?, cb?)` / `server.close()` — TCP accept loop over `TCPListen`/`TCPAccept`.
 -  `net.connect(port, host?, cb?)` → Socket; `.on('data'|'end'|'drain'|'error'|'close')`, `.write`, `.end` (FIN via `shutdown(SHUT_WR)`), `.destroy`.
 -  `dgram.createSocket(type, onMsg?)` / `.bind` / `.send(data, port, host)` / `.on('message',(msg,rinfo))` — UDP over `UDPBind`+`recvfrom`/`sendto`.
 -  `setTimeout`/`setInterval`/`clearTimeout`/`clearInterval`/`setImmediate` — a JS min-heap over the single `pol.timer`. Native leaves: `net._listen/_connect/_accept/_recv/_send/_shutwr/_close`, `dgram._bind/_recv/_send`.

###  main.cpp — context, module install, script runner

`main()` maps `ABC_BASS`, builds the context, installs utf8→io→buf→…→pol→net, runs `--eval`/script (propagating an uncaught exception to the exit code via `JABCRun`), then drains the event loop (`pol.run(pol.NEVER)`, Node-like) before releasing the context BEFORE `FILECloseAll` so GC deallocators run while the FILE subsystem is alive.

##  Tests

 -  `test/jabc_test.cpp` — table-driven C++ harness over utf8/Buf/io (in-memory rows + pipe + mmap round-trips); exits non-zero on any failed row. Build target `jabc_test`, ctest `JABCtestCpp`.
 -  ctest `JABCe2e` — the `jabc` binary runs an inline Buf+utf8 round-trip; a failed assertion throws → non-zero exit.
 -  ctest `JABCpol` — `test/pol.js`: periodic/one-shot timers, fd readiness (regular-file POLLIN, read-to-EOF drop), `pol.default`, handler-throw propagation, `pol.stop`.
 -  ctest `JABCnet` — `test/net.js`: TCP echo round-trip, a 200 KB multi-chunk transfer, UDP ping/pong, and setTimeout/clearTimeout/setInterval — all in the one implicit loop.
 -  ctest `JABCpack` — `test/pack.js`: offset-addressed pack write/seek/walk + the GIT-007 cross-impl vector (a JABC-written log resolves byte-identically through the dog/git multi-hop OFS_DELTA chase).
 -  `lsan.supp` — suppresses JSC-internal singleton leaks by library name.
