#   woof — module index

woof is the read-only HTTP front for the dog pack: it turns a `be` projector URI into a web page. A request-target is a be-URI verbatim (`GET /log:?feat` → `be log:?feat`); its scheme picks a worker, woof fork+execs `be --tlv <uri>` (or runs the dog in-process under `--api`), reads the HUNK TLV stream, renders to HTML via `HUNKu8sFeedHtml`, drains the ring out. GET/HEAD only, binds `127.0.0.1`, serves a *store*, never a worktree.

##  Server

###  WOOF.h — HTTP front singleton, slots, route table

The whole public surface: error codes, per-conn slot geometry, the `conn` record, the route table, the `WOOF` singleton, and the request-pipeline entry points tests/fuzzer drive in-process. The slot pool is one anonymous mmap of `WOOF_SLOT_BYTES` regions.

 -  `WOOFFAIL`/`BUSY`/`BADREQ`/`NOROUTE`/`FORKBAD`/`METHOD`/`CONN` — `ok64` codes, each mapping to an HTTP status.
 -  `WOOF_SLOT_BYTES`/`REQBUF`/`PIPEBUF`/`RING_BYTES`/`HUNK_HEADROOM` — the 4 MB slot split + the render idle min.
 -  `WOOF_MAX_CONNS_DEFAULT`/`PORT_DEFAULT`/`IDLE_NS` + `WOOF_VERBS`/`VAL_FLAGS` — defaults + the CLI tables.
 -  `woof_st` (`RD_HEAD`/`STREAM`/`DRAIN`/`CLOSING`) — the per-conn phases: read, pump pipe→ring, flush tail, release.
 -  `conn` — one connection: fds, slot index, `woof_st`, three `Bu8` views, two `poller`s, `HTTPstate`, `u8cs uri`.
 -  `conncmp`/`connZ` — the comparator pair the `Bconn` template requires; order by `slot`, but the array is slot-indexed.
 -  `woof_route`/`WOOF_ROUTES`/`WOOFRouteFind` — a `(scheme, binary)` row, the static safelist, and the linear lookup.
 -  `woof` (`WOOF`) — the singleton: `listen_fd`, `pool` mmap, `max_conns`, `Bconn`, `bind_addr`/`port`, mode flags.
 -  `WOOFOpen`/`WOOFClose` — allocate/release the pool mmap, conn array, listen socket, signal handlers, `--api` dogs.
 -  `WOOFExec` — dispatch the CLI: bare/`serve` runs the accept loop, `status` prints counters, `help` the usage.
 -  `WOOFApiOpen`/`Close`/`WOOFApiDogOpen`/`Run`/`Memfd` — the `--api` dispatch core: open dogs, run `Exec` capturing TLV.
 -  `WOOFutf8ExtractURI`/`woof_disp`/`ConnCarve`/`ConnRoute`/`ServeStatic`/`RenderHunks` — the fork-free pipeline.

##  Implementation units

Four `.c` units plus the CLI. **`WOOF.c`** owns the singleton lifecycle and accept loop. **`CONN.c`** is the per-connection state machine and `--api` path. **`ROUTE.c`** is `WOOF_ROUTES`/`WOOFRouteFind`. **`WOOF.cli.c`** is the thin CLI shell.

[ABC.md]: ../ABC.md
