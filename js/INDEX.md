#   js — module index

js is JABC, the JavaScriptCore bindings: a thin C++ glue layer running stock `libjavascriptcore` over the abc/POSIX stack (see [README.md]). Rule: *no shared custody* — C holds no JS refs beyond bootstrap roots, JS no C pointers; bulk data crosses as JS-owned `ArrayBuffer` bytes. The unit is [JABC.hpp] plus `.cpp` units registering one JS global each (`io`/`utf8`/`test`); a *binding* is reachable from JS, a *helper* internal C++.

##  Core & lifecycle

###  JABC.hpp — the binding header (macros + prototypes)

The one header every `.cpp` includes: abc C headers in `extern "C"`, then JavaScriptCore and `abc/ABC.hpp` under `using namespace abc`. No classes — macros plus four helper prototypes. `PRO.h` is deliberately NOT included (namespace hygiene).

 -  `JABC_CONTEXT`/`JABC_GLOBAL_OBJECT` — the `thread_local` JS global context and its object; the two roots C keeps.
 -  `JABC_FN_DEFINE(fn)` — the fixed JS-callback signature (`ctx,function,self,argc,args[],exception`).
 -  `JABC_FN_THROW(msg)`/`...RETURN_UNDEFINED`/`...CALL(f,…)` — raise a JS `Error`, bail `undefined`, invoke an `ok64` fn.
 -  `JABC_FN_ARG_STRING`/`...ARG_ALLOC_STRING` — pull arg `ndx` into a `u8cs` from a stack or `malloc`'d buffer (checked).
 -  `JABC_API_OBJECT(o)`/`JABC_API_FN(o,n,f)` — create a module object on the global and hang a named native `f` off it.
 -  `JS_MAKE_CLASS`/`JS_ADD_METHOD`/`JS_SET_PROPERTY_RO`/`JS_GET_PROPERTY` — JSC sugar for classes, methods, props.
 -  `JABCutf8bFeedValueRef`/`CopyStringValue`/`JSOfCString` — the live cross-boundary helpers (in `utf8.cpp`).
 -  `JABCExecute`/`JABCReport` — evaluate a script source and pretty-print a JS exception (in `main.cpp`).
 -  `JABCutf8cpMakeValueRef`/`JABCutf8bFeedStringRef` — prototyped here, defined nowhere; dead (Stage-2).

###  main.cpp — context lifecycle, module wiring, CLI entry

Owns the JS context's birth/death and `main`. `JSInit` creates the context and protects `message`/`stack` roots; install/uninstall bring modules up/down; `JABCExecute`/`Report` run scripts.

 -  `JSInit`/`JABCClose` — create the global context (+ protect the `message`/`stack` roots) and release on shutdown.
 -  `JABCInstallModules`/`JABCUninstallModules` — install `utf8`/`io`/`test` in order, tear down in reverse.
 -  `JABCExecute(script)` — `JSEvaluateScript` a C-string source, routing any exception to `JABCReport`.
 -  `JABCReport`/`JABCDump`/`JSObjectPropertiesDump` — format a JS exception, dump an object's enumerable props to stderr.
 -  `main` — parse `--version`/`--help`/`--eval`/`<file>`, init, install, run, then `POLLoop(POLNever)` until exit.

##  Modules (JS-visible bindings)

###  io.cpp — the `io` global: files, sockets, timers, mmap

The biggest binding surface: the `io` module plus a `File` JSC class. A `File`'s private data points into the static `JS_FILES` array and the slot index IS the fd — so no raw C pointer escapes into JS. Driven by abc's `POL` loop and `FILE`/`TCP`.

 -  `JABCioInstall`/`JABCioUninstall` — `POLInit`, allocate `JS_FILES`, define the `File` class, register, eval `io_js`.
 -  `JABCioMakeFileObject`/`FileGetDescriptor`/`CloseFile` — bind fd ↔ `File` via the `JS_FILES` slot; close on teardown.
 -  `JABCioFileWrite`/`JABCioFileRead` — `File.write`/`read` over TypedArray bytes in place, re-arming on `EAGAIN`.
 -  `JABCioStdIn`/`JABCioStdOut`/`JABCioStdErr` — `io.stdIn/Out/Err()`: the cached `File` for fd 0/1/2.
 -  `JABCioNetConnect`/`JABCioNetClose` — `io.connect(url,fn)` opens a non-blocking `TCPConnect`; `close` unprotects.
 -  `JABCioNetOnConnect`/`JABCioNetOnEvent` — poller callbacks that fire the stored callback with `"r"`/`"w"`/`"error"`.
 -  `JABCioTimer`/`JABCioWakeIn`/`JABCioNow` — `io.timer(fn)` via `POLTrackTime`, `io.wakeIn(ms)`, `io.now()` (ms).
 -  `JABCioLog`/`JABCioExit` — `io.log(…)` writes args + newline to stderr; `io.exit()` calls `exit(0)`.
 -  `JABCioFileMap`/`mmap_free` — `io.mmap(path)`: `FILEMapRO`, expose bytes as a no-copy `Uint8Array`; GC `FILEUnMap`s.
 -  `JABCioNetListen`/`JABCioNetAccept`/`JABCioNothing` — registered but stubbed (`listen`/`accept` → `undefined`).

###  utf8.cpp — the `utf8` global + UTF-8 boundary helpers

Registers `utf8.Encode`/`Decode` (with `en`/`de` aliases) and defines the cross-boundary string helpers the whole layer leans on. Home of the "UTF8/UTF16 mismatch" pain the README flags.

 -  `JABCutf8Install`/`JABCutf8Uninstall` — register the `utf8` module (`en`/`Encode`, `de`/`Decode`), cache `UTF8Object`.
 -  `JABCutf8Encode` — `utf8.Encode(string) -> Uint8Array`: copy UTF-8 into a `malloc`'d no-copy TypedArray (incl. NUL).
 -  `JABCutf8Decode` — registered but a stub (returns `undefined`).
 -  `JABCutf8bFeedValueRef`/`JSu8bString` — render a JS value/string into a `u8b` as UTF-8 (NUL trimmed).
 -  `JABCutf8CopyStringValue` — allocate and fill a `u8sp` with a JS string's UTF-8 bytes (used by `File.write`).
 -  `JSOfCString`/`JStringRefToCString`/`JAButf8CreateValue` — C string ↔ JS string; `JSOfCString` makes error messages.

###  test.cpp — the `test` global

A two-line module: create the empty `test` object and eval `test_js`, which hangs `test.is`/`test.run` off it.

 -  `JABCtestInstall`/`JABCtestUninstall` — make the `test` global object and run the embedded `test_js` glue.

##  RDX bridge (compiled, not yet wired)

###  convert.cpp — JS value ↔ RDX/TLV (currently unreachable)

Translates JS values into RDX-typed TLV and back. It compiles and links but has NO `Install` and is called from nowhere — the rdx module is commented out in `main.cpp`. Source-only machinery.

 -  `JABCu8bImport`/`...ImportString`/`...ImportObject` — recursively serialize a JS value into a `u8b` as RDX TLV.
 -  `JABCrdxExport` — the inverse: read an `rdx` record's last node and emit a JS number, else `fail(NOTIMPLYET)`.
 -  Uses abc's `call`/`done`/`fail` (it includes `PRO.h`) and `TLVu8bInto`/`TLVu8bOuto` over the builder buffer.

##  JS glue & generated blobs

###  io.js, test.js — eval'd glue (the real JS surface)

Hand-written JavaScript eval'd at install time, layering ergonomic JS over the native functions. Most of the JS-visible API lives here.

 -  `io.js` — sets `io.stdin/stdout/stderr`, builds a min-heap timer queue on `io.timer`, exposes `io.setTimeout`.
 -  `test.js` — defines `test.is(a,b)` (deep equality, path-tagged) and `test.run()` (the ctest harness).

###  js2c.js — the source-to-blob generator

A Node script (the one build-time node use). Reads a `.js`, escapes it, prints `const char* <name>_js = "…";` — i.e. the `.js.c` blobs.

 -  invoked as `node js2c.js <file.js>`; the var name is basename with `.` → `_` (`io.js` → `io_js`).

###  io.js.c, test.js.c — GENERATED string blobs

Build artifacts from `js2c.js`; each is a single `const char*` (`io_js`/`test_js`) consumed by the installers via `JABCExecute`. Do not hand-edit — regenerate from the `.js` source.

[README.md]: ./README.md
[JABC.hpp]: ./JABC.hpp
