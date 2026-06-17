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

###  main.cpp — context, module install, script runner

`main()` maps `ABC_BASS`, builds the context, installs utf8→io→buf, runs `--eval`/script (propagating an uncaught exception to the exit code via `JABCRun`), then releases the context BEFORE `FILECloseAll` so GC deallocators run while the FILE subsystem is alive.

##  Tests

 -  `test/jabc_test.cpp` — table-driven C++ harness over utf8/Buf/io (in-memory rows + pipe + mmap round-trips); exits non-zero on any failed row. Build target `jabc_test`, ctest `JABCtestCpp`.
 -  ctest `JABCe2e` — the `jabc` binary runs an inline Buf+utf8 round-trip; a failed assertion throws → non-zero exit.
 -  `lsan.supp` — suppresses JSC-internal singleton leaks by library name.
