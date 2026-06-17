#   JABC I/O API

Two entities cross the JS↔C boundary: **buffers** (bytes) and **file
descriptors** (fds). Buffers are JS-owned `Uint8Array`s wrapped in a `Buf`
cursor; fds are plain `number`s. The binding holds no memory and no JS
references beyond bootstrap — every persistent byte is owned by JS GC.

##  Principles

  - **Buffer = JS-owned bytes.** A `Buf` wraps a `Uint8Array` plus a cursor;
    the bytes are an engine `ArrayBuffer` (`io.buf`), an anonymous mapping
    (`io.ram`), or a file mapping (`io.mmap`/`io.book`). The binding allocates
    nothing that outlives a call and frees nothing by hand.
  - **fd = number.** No `File` wrapper object, no fd→object custody table. A
    fd is opened, passed to functions, and closed explicitly.
  - **The cursor lives in `Buf`, never in your code.** You call
    `feed`/`read`/`take`; the object moves its own boundaries. No offset
    arithmetic, mirroring how ABC keeps the cursor out of every C call site.
  - **Slicing is zero-copy.** `b.data()` / `b.idle()` return `Uint8Array`
    views over the same backing store. "Consume" is a view, never a C pointer.
  - **Errors throw.** A failed syscall maps `errno`→ABC `ok64`→a JS exception.
    Reads return the byte count; `0` means EOF.
  - **Copies are explicit.** Nothing crosses the boundary by value. The one
    place C memory reaches JS is a mapping (`io.mmap`/`io.ram`), handed over
    no-copy with a `munmap`/`free` deallocator the GC fires — because
    page-cache and anonymous maps can't be engine `ArrayBuffer`s.

##  The buffer model

A `Buf` is three regions over one fixed-capacity backing array, exactly ABC's
`u8b` (`abc/Bx.h`, `PAST | DATA | IDLE`):

```
  | PAST .... | DATA .... | IDLE .... |
  0          data        idle        cap
```

  - **PAST** `[0, data)` — bytes already consumed (`take`/`write`).
  - **DATA** `[data, idle)` — the live payload.
  - **IDLE** `[idle, cap)` — free space appends (`feed`/`read`) flow into.

`feed`/`read` grow DATA into IDLE; `take`/`write` shrink DATA from the PAST
side. So a `Buf` is a FIFO: fill DATA, drain DATA.

##  Construction

```js
let b = io.buf(65536);          // engine ArrayBuffer, JS-owned        (u8bAllocate)
let r = io.ram(1 << 30);        // anonymous mmap, lazy, munmap on GC  (u8bMap)
let m = io.mmap("idx", "r");    // file map RO; DATA = whole file      (FILEMapRO)
let w = io.mmap("db", "rw");    // file map RW; msync to persist       (FILEMapRW)
let c = io.mmap("new","c",4096);// create + map at size                (FILEMapCreate)
let k = io.book("log", 1<<30, 4096); // reserve 1GB VA, 4KB live, stable base (FILEBookCreate)

let v = Buf.over(u8);           // wrap existing Uint8Array as DATA (read source)
let s = Buf.into(u8);           // wrap as empty DATA / all IDLE   (write sink)
```

A file/`ram` `Buf` is the same cursor object as a heap one — only the backing
and whether it can grow differ.

##  `Buf` — feed (append into IDLE, DATA grows)

```js
b.feed(src);        // append a Uint8Array or another Buf's DATA      (u8bFeed)
b.feed1(0x0a);      // append one byte                                (u8bFeed1)
b.feedStr("foo");   // utf8-encode str straight into IDLE, advance    (encodeInto + u8bFed)
```

Each returns the byte count, or throws `NOROOM` if IDLE can't hold it (for a
growable backing, call `b.grow()` first — see below). You normally ignore the
return; the cursor advanced itself.

##  `Buf` — consume (drop from the DATA front, PAST grows)

```js
let head = b.take(n);  // view of the n consumed bytes, then advance past them (u8bUsed)
b.skip(n);             // advance past n bytes without returning a view        (u8bUsed)
```

`take` is the parser primitive: peek-and-advance in one call. The returned
view is valid until the next mutation.

##  `Buf` — views (zero-copy `Uint8Array` over the backing)

```js
b.data();   // [data, idle)  — the live payload (hand to io.write)
b.idle();   // [idle, cap)   — free space (a read target / encodeInto dst)
b.past();   // [0, data)     — already-consumed bytes
b.bytes;    // the whole backing Uint8Array
```

A view reflects the cursor at the instant you take it; after `feed`/`take`,
re-fetch. After a `grow()` that relocates the backing, old views are stale —
re-fetch.

##  `Buf` — sizes & edits

```js
b.size;     // DATA length (live bytes)        (u8bDataLen)
b.room;     // IDLE length (free bytes)        (u8bIdleLen)
b.cap;      // total capacity
b.empty;    // b.size === 0

b.shed(n);  // un-append: roll IDLE head back into DATA   (u8bShed)
b.pop();    // drop the last DATA byte                    (u8bPop)
b.reset();  // empty DATA and IDLE back to start          (u8bReset)
b.shift();  // compact: move DATA to the front, drop PAST (u8bShift)
b.splice(off, cut, paste); // in-place edit within DATA   (u8bSplice)
```

If you wrote into `b.idle()` yourself (e.g. via a foreign API), commit it with
`b.fed(n)` to advance the IDLE head by `n` (the manual `u8bFed`).

##  `Buf` — memory (growable / mapped backings only)

```js
b.grow(newCap);   // io.buf/ram: realloc-or-mremap (base may move, views detach)
                  // io.book:    extend within reserved VA (base STABLE, views survive)
b.msync();        // flush dirty pages of an RW mapping          (FILEMSync)
b.trim();         // truncate the file to DATA length            (FILETrimMap)
b.close();        // drop the mapping/alloc now (else GC does it)
```

`io.mmap(...,"r")` can't grow. `io.book` is the way to append to a file while
keeping every outstanding view valid.

##  fds

```js
let fd = io.open("data", "rw");   // "r" | "rw" | "c" (create)     (FILEOpen/Create)
io.close(fd);                                                       // (FILEClose)
io.sync(fd);                      // fsync                          (FILESync)
io.size(fd);                      // → number                      (FILESize)
io.resize(fd, n);                                                   // (FILEResize)
io.lock(fd, true); io.unlock(fd); // flock LOCK_EX / LOCK_UN        (FILELock)
let st = io.stat("data");         // {size, mtime, mode, kind}     (FILEStat)
let {pid, stdin, stdout} = io.spawn("/bin/cat", ["cat"]); // fds    (FILESpawn)
let n = io.reap(pid);             // wait, → exit code              (FILEReap)
```

##  feed/drain over fds

A `Buf` argument advances its own cursor; a bare `Uint8Array` does not (you
slice it). All return the byte count; reads return `0` at EOF.

```js
// write — drain DATA to the fd
io.write(fd, b);        // one write; consumes written bytes from DATA front (FILEFeed→u8bUsed)
io.writeAll(fd, b);     // loop until DATA is empty                          (FILEFeedAll)
io.writev(fd, [a, b]);  // scatter (writev)                                  (FILEFeedv)

// read — fill IDLE, DATA grows
io.read(fd, b);         // one read into IDLE; advances idle head            (FILEDrain→u8bFed)
io.readAll(fd, b);      // read until IDLE full or EOF                       (FILEdrainall)
io.readv(fd, [a, b]);   // gather (readv)                                    (FILEdrainv)
```

##  text

```js
utf8.encodeInto(str, dst);   // encode into a Uint8Array you own, → n   (primary, no alloc)
utf8.Encode(str);            // → fresh Uint8Array (engine-owned)       (sugar over BASS scratch)
utf8.Decode(u8);             // validate UTF-8, → JS string; throws on malformed
```

`Decode` rejects overlong forms, lone surrogates, truncated multibyte and
> U+10FFFF (via ABC `utf8sValid`/`utf8sDrain32`); it is not JSC's lenient
`JSStringCreateWithUTF8CString`.

##  Examples

```js
// stream-copy src → dst, zero offset arithmetic, one reusable buffer
let b = io.buf(1 << 16);
while (io.read(src, b) > 0)      // fill DATA from src
  while (b.size) io.write(dst, b); // drain DATA to dst
```

```js
// build a record and write it once
let b = io.buf(256);
b.feedStr("GET "); b.feed(path); b.feed1(0x0a);
io.writeAll(sock, b);
```

```js
// scan a mmapped file with no I/O and no copies
let m = io.mmap("index.u64", "r");
let view = m.data();             // Uint8Array straight over page-cache
// ... read view[...] ; bytes fault in lazily; GC munmaps when m dies
```

```js
// append to a file while keeping views valid
let k = io.book("log", 1 << 30, 4096);
k.feedStr("line\n");             // grows the mapping under the hood
if (k.room < 64) k.grow(k.cap + 4096); // stable base — k.data() stays valid
k.msync();
```

##  ABC mapping

| JS                        | ABC (`abc/Bx.h`, `abc/FILE.h`)        |
|---------------------------|----------------------------------------|
| `Buf` regions             | `u8b` `PAST \| DATA \| IDLE`            |
| `b.feed` / `feed1`        | `u8bFeed` / `u8bFeed1`                  |
| `b.feedStr`               | `utf8` encode + `u8bFed`               |
| `b.take` / `skip`         | `u8bUsed`                              |
| `b.shed` / `pop` / `reset`| `u8bShed` / `u8bPop` / `u8bReset`      |
| `b.shift` / `splice`      | `u8bShift` / `u8bSplice`               |
| `b.grow` (heap/ram)       | `u8bReMap`                             |
| `b.grow` (book)           | `FILEBookExtend`                       |
| `io.read*`                | `FILEDrain` / `FILEdrainall` / `FILEdrainv` |
| `io.write*`               | `FILEFeed` / `FILEFeedAll` / `FILEFeedv`    |
| `io.mmap` `r`/`rw`/`c`    | `FILEMapRO` / `FILEMapRW` / `FILEMapCreate`  |
| `io.book`                 | `FILEBookCreate`                       |
| `b.msync` / `trim`        | `FILEMSync` / `FILETrimMap`            |
| `io.ram`                  | `u8bMap` (anonymous)                   |
