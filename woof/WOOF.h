#ifndef WOOF_WOOF_H
#define WOOF_WOOF_H

//  WOOF — HTTP front for the dog pack.
//
//  An HTTP request's request-target is a be-URI verbatim (after a
//  leading '/' strip + percent-decode): `GET /log:?feat&n=10` is
//  `be log:?feat&n=10`.  The URI's scheme picks a worker binary from
//  WOOF_ROUTES; woof fork+execs that binary with `--tlv` and the
//  be-URI as argv.  Worker writes HUNK TLV records to its stdout;
//  woof reads them, renders each via HUNKu8sFeedHtml into a 4 MB
//  per-conn ring, drains the ring to the client socket.  Backpressure
//  composes naturally: slow client → ring fills → pipe-read pauses →
//  worker's `write(2)` blocks.
//
//  Read-only.  Only GET and HEAD are accepted; methods that would
//  mutate state respond 405.  Default bind is 127.0.0.1; --bind picks
//  another addr.  Open conns are capped at WOOF.max_conns; over-cap
//  accepts get a synchronous 503 + close.
//
//  Woof serves a repo, not a worktree.  Workers run against the
//  store (`<root>/.be/`) and resolve refs from `<branch>/refs`; no
//  `wtlog`, no sniff, no `cur_branch` follow-along.  The be-URI's
//  `?ref` slot picks the branch per request.
//
//  No persistent state, no `.be/` slot.  Singleton lives for one
//  WOOFOpen → WOOFExec → WOOFClose cycle.  The conn pool is one
//  anonymous mmap carved into MAX_CONNS fixed slots; closing a conn
//  madvise(MADV_DONTNEED)'s its slot to return physical pages while
//  keeping the virtual mapping for reuse.

#include "abc/BUF.h"
#include "abc/HTTP.h"
#include "abc/INT.h"
#include "abc/POL.h"
#include "dog/CLI.h"
#include "dog/HOME.h"

con ok64 WOOFFAIL    = 0x81860f3ca495;       // generic failure
con ok64 WOOFBUSY    = 0x81860f2de722;       // over conn cap → 503
con ok64 WOOFBADREQ  = 0x81860f2ca35b39a;    // HTTP parse failure → 400
con ok64 WOOFNOROUTE = 0x6183d761b61e74e;    // no scheme match → 404
con ok64 WOOFFORKBAD = 0x6183cf61b50b28d;    // fork/exec worker failed → 500
con ok64 WOOFMETHOD  = 0x81860f58e75160d;    // method not allowed → 405
con ok64 WOOFCONN    = 0x81860f3185d7;       // socket / pipe I/O failure

// --- Per-conn slot layout ---
//
//  Each slot is one fixed-size region inside WOOF.pool (an anonymous
//  mmap allocated at WOOFOpen).  The conn's three Bu8 fields point at
//  disjoint sub-regions of the slot:
//
//    [0,          16 KB)   in        — accumulating HTTP request
//    [16 KB,      32 KB)   pipe_in   — TLV framing from worker stdout
//    [32 KB,       4 MB)   out       — HTML output ring (sock-bound)
//
//  Total slot footprint = WOOF_SLOT_BYTES.  We never start rendering
//  a new hunk into `out` unless ≥ WOOF_HUNK_HEADROOM idle bytes
//  remain; that is the buffer-overflow guard for a single
//  HUNKu8sFeedHtml call.

#define WOOF_REQBUF_BYTES     (1UL << 14)     //  16 KB — request cap
//  TLV-aware: a worker can bundle thousands of log/grep rows into a
//  single ~hundreds-of-KB HUNK_TLV record; pipe_in must fit the
//  whole frame before HUNKu8sDrain succeeds.  1 MB covers a 9k-
//  commit log with headroom.  Producers that emit larger single
//  records will need a future incremental drain in pipe_cb.
#define WOOF_PIPEBUF_BYTES    (1UL << 20)     //   1 MB — TLV framing
#define WOOF_RING_BYTES       ((1UL << 22) - WOOF_REQBUF_BYTES - WOOF_PIPEBUF_BYTES)
#define WOOF_SLOT_BYTES       (1UL << 22)     //   4 MB
#define WOOF_HUNK_HEADROOM    (1UL << 14)     //  16 KB — render guard

#define WOOF_MAX_CONNS_DEFAULT 64
#define WOOF_PORT_DEFAULT      8080
#define WOOF_IDLE_NS           (30UL * POLNanosPerSec)

// --- Per-conn state ---

typedef enum {
    WOOF_RD_HEAD = 0,    // draining client → in, awaiting full request
    WOOF_STREAM,         // pumping pipe → out → sock
    WOOF_DRAIN,          // pipe EOF; flushing tail bytes in out
    WOOF_CLOSING,        // socket drained or fault; release on next tick
} woof_st;

typedef struct {
    int       sock_fd;     // accepted client socket; -1 = free slot
    int       pipe_fd;     // worker stdout (read end); -1 until fork
    int       worker_pid;  // -1 until fork; reaped via SIGCHLD path
    u32       slot;        // index into WOOF.pool — for MADV_DONTNEED
    woof_st   state;
    u64       last_io_ns;  // idle-timeout anchor

    poller    pol_sock;    // POLLIN (read req) / POLLOUT (drain ring)
    poller    pol_pipe;    // POLLIN — pull hunks from worker

    Bu8       in;          // view into slot [0, 16 KB)
    Bu8       pipe_in;     // view into slot [16 KB, 32 KB)
    Bu8       out;         // view into slot [32 KB, 4 MB)

    HTTPstate req;         // parsed request (slices into `in`)
    u8cs      uri;         // be-URI carved from req.uri (post-decode)
} conn;

//  Required by the Bx.h template.  Conns are kept in a fixed array
//  indexed by slot; ordering doesn't matter, just match by slot.
fun int conncmp(conn const *a, conn const *b) {
    return (a->slot < b->slot) ? -1 : (a->slot > b->slot);
}
fun b8 connZ(conn const *a, conn const *b) {
    return a->slot < b->slot;
}

#define X(M, name) M##conn##name
#include "abc/Bx.h"
#undef X

// --- Route table ---
//
//  Maps a be-URI scheme to the worker binary that handles it.  An
//  empty `scheme` matches the verbless case (file viewer → bro).  The
//  table is terminated by an entry whose `binary` slice is empty.
//
//  Lookup is a linear scan in WOOFRouteFind; the table is small (one
//  row per projector scheme) and read-mostly.  Worker binary names
//  are resolved against the running woof's directory via
//  HOMEResolveSibling at fork time — same convention spot uses for
//  bro.

typedef struct {
    u8cs scheme;     // URI scheme (e.g. "log", "blob"); empty = verbless
    u8cs binary;     // worker basename (e.g. "keeper", "graf", "bro")
} woof_route;

extern woof_route const WOOF_ROUTES[];

//  Find the route for `scheme` (empty matches the verbless row).
//  Returns NULL on miss — caller responds 404 (WOOFNOROUTE).
woof_route const *WOOFRouteFind(u8cs scheme);

// --- Singleton state ---

typedef struct {
    home *h;             // borrowed
    int   listen_fd;     // bound TCP socket; -1 until WOOFOpen
    u8   *pool;          // mmap(MAX_CONNS * WOOF_SLOT_BYTES); NULL pre-Open
    u32   max_conns;     // slot capacity; default WOOF_MAX_CONNS_DEFAULT
    u32   live_conns;    // running count of in-use slots
    Bconn conns;         // per-slot metadata; index ≡ pool slot
    u8cs  bind_addr;     // borrowed from argv; default "127.0.0.1"
    u16   port;          // default WOOF_PORT_DEFAULT
    b8    rw;             // YES → enable mutating verbs (reserved; v1=NO)
    b8    api;            // YES (`--api`) → dispatch projector schemes
                         // in-process (library call) instead of fork+
                         // exec'ing a worker.  Single project, opened
                         // once at serve start (WOOFApiOpen).  Schemes
                         // without an in-process path still fork.
} woof;

extern woof WOOF;

// --- Public API ---

ok64 WOOFOpen(home *h);
ok64 WOOFClose(void);

ok64 WOOFExec(cli *c);

//  In-process projection dispatch (`--api` mode).  WOOFApiOpen opens
//  the projector dog(s) once against WOOF.h's project (single project;
//  multi-project dispatch is a TODO — see WOOF.cli.c); WOOFApiClose
//  releases them.  Implemented in CONN.c (it links the dog libs); no-op
//  unless WOOF.api.  Called by woof_serve / WOOFClose.
ok64 WOOFApiOpen(void);
void WOOFApiClose(void);

//  Reusable in-process dispatch core (shared by the server and the fuzz
//  harness).  WOOFApiDogOpen reports whether the dog owning a scheme was
//  opened; WOOFApiRun runs that dog's Exec on a verbless projector URI,
//  capturing its TLV output into the fd returned by WOOFApiMemfd (read
//  it after the call).  Caller gates on WOOFApiDogOpen first.
b8   WOOFApiDogOpen(char const *dog);
ok64 WOOFApiRun(uri *u, char const *dog);
int  WOOFApiMemfd(void);

//  Verb + value-flag tables for CLIParse.
extern char const *const WOOF_VERBS[];     // "serve", "status", NULL
extern char const WOOF_VAL_FLAGS[];        // "bp" — --bind, --port

// --- Request pipeline (shared by CONN.c read_cb, tests, fuzz) ---
//
//  These three carry the URI-parse-and-dispatch path that read_cb
//  runs after the HTTP layer hands it a request-target.  They are
//  fork-free and side-effect-light (the only resource serve_static
//  touches is a balanced mmap), so woof/fuzz can drive the exact same
//  parsing in-process — "dispatch at API level, not by proc fork".

//  Routing verdict from WOOFConnRoute.  No fork, no socket writes.
typedef enum {
    WOOF_DISP_ERROR  = 0,   // *err set (WOOFBADREQ→400 / WOOFNOROUTE→404)
    WOOF_DISP_STATIC,       // served inline into c->out (state := DRAIN)
    WOOF_DISP_WORKER,       // *route set; caller forks route->binary
} woof_disp;

//  Strip the leading '/' from `target` and percent-decode the rest
//  into `out` (reset first).  WOOFBADREQ on missing slash / bad '%XX'.
ok64 WOOFutf8ExtractURI(Bu8 out, u8cs target);

//  Carve a slot's 4 MB at WOOF.pool[c->slot * WOOF_SLOT_BYTES] into the
//  conn's in / pipe_in / out views.  WOOF.pool must be mapped.
void WOOFConnCarve(conn *c);

//  Decide what to do with a request whose `c->uri` already holds the
//  decoded be-URI: serve a static asset inline (fills c->out), route
//  to a worker (sets *route), or fail (sets *err).  Mirrors read_cb's
//  parse path verbatim, minus the worker fork.
woof_disp WOOFConnRoute(conn *c, woof_route const **route, ok64 *err);

#endif
