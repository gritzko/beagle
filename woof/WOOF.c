//  woof/WOOF.c — singleton lifecycle, listen socket, accept loop.
//
//  This file owns:
//    * The WOOF singleton (pool mmap, conn metadata array, listen fd).
//    * The TCP listen socket and its accept callback.
//    * SIGCHLD-driven worker reaping (timer-paced; no signal handler
//      work beyond setting a flag).
//
//  Per-conn state lives in woof/CONN.c — request parsing, worker
//  fork/exec, hunk-pipe drain, ring → socket writer.  The three
//  WOOFConn* entry points below are extern; the no-op fallbacks at
//  the bottom of this file exist so WOOF.c compiles + links
//  standalone during the scaffold phase and are removed when CONN.c
//  lands.

#include "WOOF.h"

#include "abc/FILE.h"
#include "abc/NET.h"
#include "abc/PRO.h"
#include "abc/TCP.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

woof WOOF = {};

// --- CONN entry points (defined in woof/CONN.c) ---

//  Take ownership of an accepted client socket: claim a slot, kick
//  off the read state machine, register pollers.  Returns OK iff the
//  conn was accepted; non-OK means the caller (accept_cb) must close
//  `cfd` itself.
extern ok64 WOOFConnAccept(int cfd);

//  Tear down every live conn: kill workers, drain or close sockets,
//  release slots.  Called from WOOFClose; safe to invoke when no
//  conns are live.
extern void WOOFConnCloseAll(void);

//  Drain pending child exits via waitpid(WNOHANG).  Called from the
//  POL timer tick whenever the SIGCHLD flag is set.
extern void WOOFConnReapAll(void);

// --- 503 fast-reject for over-cap accepts ---

static char const WOOF_REFUSE_503[] =
    "HTTP/1.1 503 Service Unavailable\r\n"
    "Retry-After: 1\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n\r\n";

// --- pool ---

static ok64 pool_alloc(u32 max_conns) {
    sane(max_conns > 0);
    size_t bytes = (size_t)max_conns * WOOF_SLOT_BYTES;
    void *p = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) fail(WOOFFAIL);
    WOOF.pool = (u8 *)p;
    done;
}

static void pool_free(void) {
    if (WOOF.pool == NULL) return;
    size_t bytes = (size_t)WOOF.max_conns * WOOF_SLOT_BYTES;
    (void)munmap(WOOF.pool, bytes);
    WOOF.pool = NULL;
}

// --- accept ---

static short accept_cb(int sfd, poller *p) {
    (void)p;
    int cfd = -1;
    aNETraw(caddr);
    if (TCPAccept(&cfd, caddr, sfd) != OK) {
        return POLLIN;  //  spurious / EAGAIN: stay registered, retry
    }

    if (WOOF.live_conns >= WOOF.max_conns) {
        (void)write(cfd, WOOF_REFUSE_503, sizeof(WOOF_REFUSE_503) - 1);
        (void)TCPClose(cfd);
        return POLLIN;
    }

    if (WOOFConnAccept(cfd) != OK) {
        (void)TCPClose(cfd);
    }
    return POLLIN;
}

// --- SIGCHLD plumbing ---

static volatile sig_atomic_t woof_sigchld_pending = 0;

static void sigchld_handler(int signo) {
    (void)signo;
    woof_sigchld_pending = 1;
}

static void install_signals(void) {
    struct sigaction sa = {};
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDSTOP | SA_RESTART;
    (void)sigaction(SIGCHLD, &sa, NULL);
    //  Worker pipe writes can race a closed socket; ignore SIGPIPE so
    //  the affected read/write returns EPIPE instead.
    (void)signal(SIGPIPE, SIG_IGN);
}

static u32 woof_timer_cb(u64 ns) {
    (void)ns;
    if (woof_sigchld_pending) {
        woof_sigchld_pending = 0;
        WOOFConnReapAll();
    }
    //  Re-arm every second; per-conn idle deadlines (WOOF_IDLE_NS)
    //  travel inside each conn's own poller.
    return 1000;
}

// --- Open / Close ---

ok64 WOOFOpen(home *h) {
    sane(h);
    if (WOOF.pool != NULL) return WOOFFAIL;  //  already open

    zerop(&WOOF);
    WOOF.h         = h;
    WOOF.listen_fd = -1;
    WOOF.max_conns = WOOF_MAX_CONNS_DEFAULT;
    WOOF.port      = WOOF_PORT_DEFAULT;
    {
        a$str(loopback, "127.0.0.1");
        WOOF.bind_addr[0] = loopback[0];
        WOOF.bind_addr[1] = loopback[1];
    }
    WOOF.rw = NO;

    call(pool_alloc, WOOF.max_conns);
    call(connbAllocate, WOOF.conns, WOOF.max_conns);

    //  Pre-fill the slot table; CONN claims a slot per accept.
    for (u32 i = 0; i < WOOF.max_conns; i++) {
        conn z = {};
        z.sock_fd    = -1;
        z.pipe_fd    = -1;
        z.worker_pid = -1;
        z.slot       = i;
        z.state      = WOOF_RD_HEAD;
        call(connbFeed1, WOOF.conns, z);
    }

    install_signals();
    done;
}

ok64 WOOFClose(void) {
    sane(1);
    if (WOOF.pool == NULL) return OK;

    //  Stop accepting new clients before tearing down anything else.
    if (WOOF.listen_fd >= 0) {
        (void)POLIgnoreEvents(WOOF.listen_fd);
        (void)TCPClose(WOOF.listen_fd);
        WOOF.listen_fd = -1;
    }

    WOOFConnCloseAll();
    WOOFApiClose();           //  release in-process dog(s); no-op if !api
    connbFree(WOOF.conns);
    pool_free();
    zerop(&WOOF);
    done;
}

// --- serve verb ---

//  Parse a base-10 unsigned port (1..65535).  Returns OK + writes
//  `*out`; refuses empty / non-numeric / out-of-range with WOOFBADREQ.
static ok64 parse_port(u16 *out, u8cs s) {
    sane(out);
    if ($empty(s)) fail(WOOFBADREQ);
    u32 v = 0;
    $for(u8c, q, s) {
        if (*q < '0' || *q > '9') fail(WOOFBADREQ);
        v = v * 10 + (u32)(*q - '0');
        if (v > 65535) fail(WOOFBADREQ);
    }
    if (v == 0) fail(WOOFBADREQ);
    *out = (u16)v;
    done;
}

//  Compose `tcp://<bind>:<port>` for TCPListen.  Caller passes a
//  pre-sized stack u8b (≥ 64 bytes is comfortable).
static ok64 compose_listen_uri(u8b out, u8cs bind, u16 port) {
    sane(1);
    u8bReset(out);
    {
        a$str(prefix, "tcp://");
        call(u8bFeed, out, prefix);
    }
    call(u8bFeed, out, bind);
    call(u8bFeed1, out, ':');
    {
        //  Fixed-width buffer; max u16 = 5 digits.
        char num[8];
        int n = snprintf(num, sizeof(num), "%u", (unsigned)port);
        if (n <= 0) fail(WOOFFAIL);
        u8cs ns = {(u8c *)num, (u8c *)num + n};
        call(u8bFeed, out, ns);
    }
    done;
}

static ok64 woof_serve(cli *c) {
    sane(c);

    //  --bind / --port overrides; defaults already in WOOF.
    {
        u8cs bind = {};
        CLIFlag(bind, c, "--bind");
        if (bind[0] != NULL && !u8csEmpty(bind)) {
            WOOF.bind_addr[0] = bind[0];
            WOOF.bind_addr[1] = bind[1];
        }
    }
    {
        u8cs port_s = {};
        CLIFlag(port_s, c, "--port");
        if (port_s[0] != NULL && !u8csEmpty(port_s)) {
            u16 p = 0;
            call(parse_port, &p, port_s);
            WOOF.port = p;
        }
    }

    //  --api: dispatch projector schemes in-process (library call) for
    //  the dogs that support it; the rest still fork.  Open the dog set
    //  once now; on failure, warn and fall back to fork-only so the
    //  server still serves.
    if (CLIHas(c, "--api")) {
        WOOF.api = YES;
        if (WOOFApiOpen() != OK) {
            fprintf(stderr, "woof: --api: in-process open failed; "
                            "falling back to fork dispatch\n");
            WOOF.api = NO;
        }
    }

    a_pad(u8, addrbuf, 64);
    call(compose_listen_uri, addrbuf, WOOF.bind_addr, WOOF.port);
    a_dup(u8c, addr, u8bData(addrbuf));

    //  Per-conn slots = max_conns; each conn occupies up to 2 fds
    //  (sock + worker pipe) plus the listen fd + the timer slot.
    call(POLInit, (int)(WOOF.max_conns * 2 + 4));
    call(TCPListen, &WOOF.listen_fd, addr);
    //  abc/POL's "deliver timeouts" pass fires the listen-fd callback
    //  whenever its 1-second deadline lapses without an I/O event.
    //  If accept(2) blocks, the whole loop wedges.  O_NONBLOCK turns
    //  the spurious timeout fire into an EAGAIN that accept_cb ignores.
    (void)fcntl(WOOF.listen_fd, F_SETFL,
                fcntl(WOOF.listen_fd, F_GETFL, 0) | O_NONBLOCK);

    poller listen_pol = {
        .callback = accept_cb,
        .events   = POLLIN,
        .tofd     = WOOF.listen_fd,
    };
    call(POLTrackEvents, WOOF.listen_fd, listen_pol);
    call(POLTrackTime, woof_timer_cb);

    fprintf(stderr, "woof: listening on tcp://%.*s:%u (cap %u)\n",
            (int)$size(WOOF.bind_addr), (char const *)WOOF.bind_addr[0],
            (unsigned)WOOF.port, (unsigned)WOOF.max_conns);

    //  Block until POLStop (signal handler / fault).
    return POLLoop(POLNever);
}

static ok64 woof_status(cli *c) {
    (void)c;
    sane(1);
    fprintf(stdout,
            "woof: listen_fd=%d port=%u max_conns=%u live=%u\n",
            WOOF.listen_fd, (unsigned)WOOF.port,
            (unsigned)WOOF.max_conns, (unsigned)WOOF.live_conns);
    done;
}

// --- Exec ---

ok64 WOOFExec(cli *c) {
    sane(c);

    a_cstr(v_serve,  "serve");
    a_cstr(v_status, "status");
    a_cstr(v_help,   "help");

    if (CLIHas(c, "-h") || CLIHas(c, "--help") || $eq(c->verb, v_help)) {
        fprintf(stderr,
                "usage: woof [serve|status]  [--bind ADDR] [--port N] [--api]\n"
                "  --api   dispatch projector schemes in-process (no fork)\n");
        done;
    }

    //  Bare `woof` ≡ `woof serve` — same idiom as bare `be` → status.
    if ($empty(c->verb) || $eq(c->verb, v_serve)) return woof_serve(c);
    if ($eq(c->verb, v_status))                    return woof_status(c);

    fail(WOOFFAIL);
}

// --- CLI tables (consumed by CLIParse in WOOF.cli.c) ---

char const *const WOOF_VERBS[] = {
    "serve", "status", "help", NULL
};

char const WOOF_VAL_FLAGS[] =
    "--bind\0--port\0";

// --- Scaffold stubs (REMOVE WHEN woof/CONN.c LANDS) ---
//
//  These three weak no-ops let WOOF.c compile and link standalone.
//  Real implementations live in woof/CONN.c — once that file is
//  added to the CMakeLists, delete this block.

__attribute__((weak)) ok64 WOOFConnAccept(int cfd) {
    static char const todo[] =
        "HTTP/1.1 501 Not Implemented\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: 27\r\n"
        "Connection: close\r\n\r\n"
        "woof/CONN.c not wired yet\n";
    (void)write(cfd, todo, sizeof(todo) - 1);
    (void)TCPClose(cfd);
    return OK;
}

__attribute__((weak)) void WOOFConnCloseAll(void) {}
__attribute__((weak)) void WOOFConnReapAll(void) {}
