# ABC refactoring policies

Bullet-form one-pager for converting raw C into ABC idioms. References
the canonical docs — read them for full coverage:

- `CLAUDE.md` — coding guidelines (esp. §1, §5, §10, §16)
- `abc/S.md` — slice types and typed-function table
- `abc/B.md` — buffer types and operations
- `abc/INDEX.md` — boundary-movers and module index
- `dog/INDEX.md` — tokenizer + dog/ shared infra

## Slice / buffer primitives

- **Default writes — `sFeed` / `sFeed1`** (all-or-nothing, returns
  `SNOROOM` if input doesn't fit). Examples: `u64sFeed1(slice, val)`,
  `u8sFeed(into, from)`.
- **Chunked / partial writes — `sDrain` / `sDrain1`** (write what
  fits, advance both sides). Use when input may exceed remaining
  room and progress matters.
- **Set a slice's range — `a_dup(T, name, src)`** or `*sMv(dst,
  src)` (e.g. `u8csMv`, `u8sMv`). Never `slice[0] = ptr; slice[1] =
  ptr + len;`.
- **Stack buffer — `a_pad(T, name, len)`**, not raw `T name[len]`.
- **C-string-literal slice — `a_cstr(name, "lit")`**.
- **Iteration — `$for` / `$rof` / `$eat`**, `sUsed1` / `sUsed`,
  `sShed` (S.md). No raw `*p++`, `ptr + N`, `end - start`.
- **Typed > generic** (CLAUDE.md §1, S.md): `u8csLen` over `$len`,
  `u8bReset` over `Breset`, `u8sMv` over `$mv`. Reach for a generic
  only when the typed form doesn't exist.
- **Reference for boundary movers** (push/feed/used/shed for both
  slices and buffers): `abc/INDEX.md` §"Slice / buffer boundary
  movers".
- special mention: it is *very very bad* to construct slices using
  pointer arithmetics, pass them to slice based API, then drop or
  rip apart into pointers. *Never ever do that!* Slice API is nice
  as long as you dont switch back and forth between modes. Mode
  switching creates impedance mismatch and *makes things worse*.
- when buffer/slice *may* potentially turn short, use call(u8bFeed...)
  call(u8sFeed...) etc to prevent garbage/incomplete data. This
  does not apply with explicit const lengths only, then just u8sFeed()

## BASS scratch — `a_carve` and friends

Per-call scratch comes from **ABC_BASS**, a process-wide arena. The
PRO.h macros acquire from it implicitly; everything acquired in a
procedure dies at the surrounding `call()` / `try()` boundary — no
manual `u8bFree` / `u8bUnMap`. Prefer these over heap `u8bAllocate` or
fixed `u8 pad[N]` for scratch (fixed heap bufs overflow to NOROOM).

- **`a_carve(T, name, cap)`** — acquire a writable buffer of `cap`
  elements (the workhorse, e.g. `a_carve(u8, buf, 1UL<<20)`). Use for
  scratch you feed into.
- **`a_lign(T, gauge)`** … **`a_cquire(T, slice)`** — open a gauge on
  BASS, fill it, then close it into a slice. The bracketing pair for
  "build a slice of unknown length in place".
- **`a_rent(T, news, src)` / `a_ren(news, src)`** — one-shot copy of
  `src` into a fresh BASS slice `news` (`a_ren` = the `u8` case).
- Stack-only (no BASS): **`a_pad(T, name, len)`** (fixed `T name[len]`
  buffer), **`a_dup(T, name, src)`** (alias a slice), **`a_cstr(name,
  "lit")`** (literal slice), **`a_path(name, …)`** (path buffer).

Rules (PRO.h §"BASS-implicit arena macros"):
- They invoke the underlying op **directly**, not via `call()` —
  `call()` snapshots+restores BASS and would undo the acquisition.
  Acquire in the op's own `sane()` frame; a `call(...)` rewinds BASS to
  its entry, freeing whatever the callee carved (so a per-iteration
  `call`/`try` frees that iteration's carves).
- Errors still propagate via `__` + `return __`, exactly like `call()`.
- Do **not** `a_carve` / `a_rent` between an `a_lign` and its matching
  `a_cquire` — those advance both DATA and IDLE and corrupt the
  in-flight gauge (a `must()` guard enforces DATA-empty at entry).
- A carve used as a **hash set** (`wh128` etc.) must `zerob(name)` after
  acquire — BASS reuses memory that carries leftover bytes from
  previously-rewound carves (see `graf/INDEX.md`).

## Paths

- **`path8b`** — owned path buffer, **NUL-terminated by
  construction**. The right type whenever code currently materialises
  a `char[N]` to satisfy a NUL contract.
- **`a_path(name, base?, seg…)`** — stack path buffer, optionally
  pre-fed.
- **Append** with `PATHu8bFeed`, `PATHu8bAdd`, `PATHu8bPush`.
- **`$path(buf)`** — `u8cs` view over a path buffer's NUL-terminated
  DATA. Use this when handing a `path8b` to a function expecting
  `u8cs`.
- **Heap pair** — `PATHu8bAlloc` / `PATHu8bFree` (4 KiB).

## Hashes / hashlets

- Use **`sha1`** (20 raw bytes) and **`sha1hex`** (40 hex ASCII)
  from `dog/WHIFF.h`. Conversions: `sha1hexFromSha1`,
  `sha1FromSha1hex`, `sha1hexFromHex`, `sha1hexSlice`.
- **Hashlet — 6..40 hex chars.** URI fragments may carry any prefix
  in this range; `len != 40` is too strict.
- Don't carry SHAs as raw `u8[20]` / `char[40]` plus manual length.

## Zero-init

- **`zero(x)`** / **`zerop(p)`** from `abc/S.h`. Don't hand-roll
  `memset(&x, 0, sizeof(x))` or `memset(p, 0, sizeof(*p))`.
  Also, typed fns are preferred, e.g. `sha1Zero(&hash)`

## Don't copy if you can pass the slice

- Pattern `memcpy(buf, slice[0], len); buf[len]=0; foo(buf)` is a
  refactor candidate. Change `foo`'s signature to take `u8csc` (or
  `path8sc`) and pass the slice directly. The truncate-cap +
  `memcpy` + manual NUL goes away — and so does the hidden info
  loss when paths exceed `FILE_PATH_MAX_LEN`.
- For NUL-terminated logging APIs that took `const char *line`,
  switch to `u8csc line` and format with `U8SFMT` / `u8sFmt`.

## Never ever do manual parsing in C

Never ever do manual parsing in C
Never ever do manual parsing in C
Never ever do manual parsing in C
Never ever do manual parsing in C
Use ragel parsers for that

## Resource lifecycle (CLAUDE.md §5)

- **Alloc / mmap / open at the top of the call chain.** Worker
  functions receive resources, never own them.
- For `cli`-shaped main entry points the canonical pattern is
  wrapper + worker so a single allocation/free pair covers every
  exit path the worker takes:
  ```c
  static ok64 xxxcli_inner(cli *c) { /* call(...)/done; throughout */ }

  ok64 xxxcli() {
      sane(1);
      cli c = {};
      call(PATHu8bAlloc, c.repo);
      try(xxxcli_inner, &c);
      PATHu8bFree(c.repo);
      done;
  }
  ```
  `try` runs the worker without short-circuiting; `done;` returns
  the worker's status after cleanup.
  Note that a buffer (e.g. u8b or sha1b) implies memory ownership.
  Buffers are not copies (pass a gauge u8g or u8bp instead).

## PRO.h flow (CLAUDE.md §1, §16)

- Functions using `call`/`done` must start with **`sane(...)`** —
  it declares the implicit `__` carrier. Two `sane()` calls in the
  same scope re-declare; combine the conditions.
- **`call(f, …)`** — invoke; on non-OK, propagate via early return.
- **`try(f, …)`** — invoke; capture status without returning. Pair
  with `then` / `nedo` / `on(code)` or proceed to cleanup + `done;`.
- **`fail(code)`** — return that code with a trace.
- **`MAIN(fn)` / `TEST(fn)` / `FUZZ(...)`** declare PRO.h globals;
  non-MAIN entry points must not use PRO.h macros.
- **PRO.h must not be included by other headers** (CLAUDE.md §6) —
  its macros pollute namespaces. `.c` files only.
- error codes are RON base64 constrants, max 60 bits, 10 chars,
  abc/ok64 generates the boilerplate

## Naming (CLAUDE.md §2)

- `MOD typ8 VerbStuff` — e.g. `HEXu8sFeed`, `KEEPSync`,
  `URIutf8Drain`.
- ABC record types end with bit width: `sha256`, `u64`, `tok32`.
- `Stuff` is combinatorial flavor (`FeedSome`, `DrainAll`,
  `FromHex`).
- Error codes: uppercase, RON60-encoded — see `abc/ok64`.

## When refactoring (workflow)

- **Read `INDEX.md` files first** (CLAUDE.md §13, §14): a helper
  may already exist. Check `abc/INDEX.md`, `dog/INDEX.md`, and the
  module's own `INDEX.md` before adding anything.
- **Slice consumption is signalled by type.** `u8cs` (consumed
  drain target) vs `u8csc` (immutable input) — choose accordingly.
- **Trace bug fixes back to a repro test** (CLAUDE.md §17): make
  the failing case first, then fix.
- **Update the relevant `INDEX.md`** when you add or rename
  headers (CLAUDE.md §14).
