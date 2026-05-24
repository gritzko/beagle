//  woof/CONN.c — per-connection state machine.
//
//  Three pollers per live conn:
//    1. sock POLLIN  — read_cb  drains the HTTP request into c->in.
//    2. pipe POLLIN  — pipe_cb  drains TLV hunks from the worker into
//                       c->pipe_in, renders them as HTML into c->out
//                       (while c->out has ≥ WOOF_HUNK_HEADROOM idle).
//    3. sock POLLOUT — write_cb drains c->out to the client.
//
//  Lifecycle:  read_cb forks the worker, swaps the sock poller from
//  POLLIN to POLLOUT, and registers the pipe poller.  On worker EOF,
//  pipe_cb emits the HTML postlude into c->out and flips state to
//  DRAIN; the next write_cb that finds c->out empty releases the slot.
//
//  Backpressure is OS-shaped: a slow client → c->out fills → we drop
//  POLLIN on the pipe → kernel pipe buffer fills → worker write(2)
//  blocks.  No explicit flow control.
//
//  The slot's 4 MB lives at WOOF.pool[slot * WOOF_SLOT_BYTES] and is
//  returned to the kernel via madvise(MADV_DONTNEED) on slot_release;
//  the virtual mapping stays in place for the next conn that lands
//  on the same slot.

#include "WOOF.h"

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/HTTP.h"
#include "abc/PRO.h"
#include "abc/TCP.h"
#include "abc/URI.h"
#include "dog/HOME.h"
#include "dog/HUNK.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

extern woof WOOF;

//  argv[0] for HOMEResolveSibling.  WOOF.cli.c assigns it; if unset,
//  HOMEResolveSibling falls back to PATH-search on the bare basename.
char const *WOOF_ARGV0 = NULL;

// --- byte-literal slice helper ---

#define LIT(s) ((u8cs){(u8c *)(s), (u8c *)(s) + sizeof(s) - 1})

// --- fixed responses ---

static char const RESP_200_HEAD[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Cache-Control: no-store\r\n"
    "Connection: close\r\n\r\n";

static char const HTML_PRELUDE_HEAD[] =
    "<!doctype html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "<meta charset=\"utf-8\">\n"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
    "<title>woof ";

static char const HTML_PRELUDE_TAIL[] =
    "</title>\n"
    "<link rel=\"stylesheet\" href=\"/static/style.css\">\n"
    "</head>\n"
    "<body>\n";

static char const HTML_POSTLUDE[] =
    "</body>\n"
    "</html>\n";

static char const RESP_400[] =
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Type: text/plain; charset=utf-8\r\n"
    "Content-Length: 12\r\n"
    "Connection: close\r\n\r\n"
    "bad request\n";

static char const RESP_404[] =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: text/plain; charset=utf-8\r\n"
    "Content-Length: 10\r\n"
    "Connection: close\r\n\r\n"
    "not found\n";

static char const RESP_405[] =
    "HTTP/1.1 405 Method Not Allowed\r\n"
    "Allow: GET, HEAD\r\n"
    "Content-Type: text/plain; charset=utf-8\r\n"
    "Content-Length: 19\r\n"
    "Connection: close\r\n\r\n"
    "method not allowed\n";

static char const RESP_500[] =
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Content-Type: text/plain; charset=utf-8\r\n"
    "Content-Length: 14\r\n"
    "Connection: close\r\n\r\n"
    "server error\n";

// --- slot allocation ---

//  Carve c->in, c->pipe_in, c->out into the three sub-regions of the
//  slot's 4 MB at WOOF.pool[c->slot * WOOF_SLOT_BYTES].
static void slot_carve_views(conn *c) {
    u8 *base = WOOF.pool + (size_t)c->slot * WOOF_SLOT_BYTES;
    u8 *p_in  = base;
    u8 *p_pin = p_in  + WOOF_REQBUF_BYTES;
    u8 *p_out = p_pin + WOOF_PIPEBUF_BYTES;
    c->in[0]      = p_in;   c->in[1]      = p_in;
    c->in[2]      = p_in;   c->in[3]      = p_in  + WOOF_REQBUF_BYTES;
    c->pipe_in[0] = p_pin;  c->pipe_in[1] = p_pin;
    c->pipe_in[2] = p_pin;  c->pipe_in[3] = p_pin + WOOF_PIPEBUF_BYTES;
    c->out[0]     = p_out;  c->out[1]     = p_out;
    c->out[2]     = p_out;  c->out[3]     = p_out + WOOF_RING_BYTES;
}

static conn *slot_claim(int sock_fd) {
    conn *cs = connbDataHead(WOOF.conns);
    for (u32 i = 0; i < WOOF.max_conns; i++) {
        if (cs[i].sock_fd < 0 && cs[i].pipe_fd < 0 && cs[i].worker_pid <= 0) {
            conn *c = &cs[i];
            u32 slot = c->slot;
            *c = (conn){};
            c->slot       = slot;
            c->sock_fd    = sock_fd;
            c->pipe_fd    = -1;
            c->worker_pid = -1;
            c->state      = WOOF_RD_HEAD;
            c->last_io_ns = POLNow();
            slot_carve_views(c);
            WOOF.live_conns++;
            return c;
        }
    }
    return NULL;
}

//  When `from_cb` is YES, the caller is running inside a POL
//  callback and one of the fds about to be closed is the one driving
//  that callback.  We skip POLIgnoreEvents on those fds — POLLoop
//  ejects via the return value — and only POLIgnoreEvents fds that
//  belong to OTHER callbacks.
static void slot_release_x(conn *c, b8 from_cb, int safe_fd) {
    if (c->pipe_fd >= 0) {
        if (!from_cb || c->pipe_fd != safe_fd) {
            (void)POLIgnoreEvents(c->pipe_fd);
        }
        (void)close(c->pipe_fd);
        c->pipe_fd = -1;
    }
    if (c->sock_fd >= 0) {
        if (!from_cb || c->sock_fd != safe_fd) {
            (void)POLIgnoreEvents(c->sock_fd);
        }
        (void)TCPClose(c->sock_fd);
        c->sock_fd = -1;
    }
    if (c->worker_pid > 0) {
        //  Best-effort SIGTERM; reaper drains the corpse.
        (void)kill(c->worker_pid, SIGTERM);
        c->worker_pid = -1;
    }
    u8 *base = WOOF.pool + (size_t)c->slot * WOOF_SLOT_BYTES;
    (void)madvise(base, WOOF_SLOT_BYTES, MADV_DONTNEED);
    u32 slot = c->slot;
    *c = (conn){};
    c->slot       = slot;
    c->sock_fd    = -1;
    c->pipe_fd    = -1;
    c->worker_pid = -1;
    c->state      = WOOF_RD_HEAD;
    if (WOOF.live_conns > 0) WOOF.live_conns--;
}

//  Safe to call from outside any POL callback (WOOFConnCloseAll
//  path).  Inside callbacks use slot_release_x with from_cb=YES.
static void slot_release(conn *c) { slot_release_x(c, NO, -1); }

// --- inline error reply ---

static void send_and_close(int fd, char const *p, size_t n) {
    size_t off = 0;
    while (off < n) {
        ssize_t w = write(fd, p + off, n - off);
        if (w <= 0) {
            if (w < 0 && (errno == EINTR)) continue;
            break;
        }
        off += (size_t)w;
    }
    (void)TCPClose(fd);
}

// --- static-asset serving (.be/static/) ---

//  Pick a Content-Type from a filename's trailing extension.  Returns
//  a NUL-terminated C string (static literal) suitable for plopping
//  into the HTTP header verbatim.  We only need text/css for the
//  stylesheet today; broaden the table as more asset types arrive.
static char const *mime_for(u8cs name) {
    u8c *dot = NULL;
    for (u8c *p = name[0]; p < name[1]; p++)
        if (*p == '.') dot = p;
    if (dot == NULL) return "application/octet-stream";
    u8cs ext = { dot + 1, name[1] };
    a$str(css,  "css");
    a$str(html, "html");
    a$str(js,   "js");
    a$str(svg,  "svg");
    a$str(png,  "png");
    if ($eq(ext, css))  return "text/css; charset=utf-8";
    if ($eq(ext, html)) return "text/html; charset=utf-8";
    if ($eq(ext, js))   return "application/javascript; charset=utf-8";
    if ($eq(ext, svg))  return "image/svg+xml";
    if ($eq(ext, png))  return "image/png";
    return "application/octet-stream";
}

//  Refuse traversal / hidden segments in `path`.  PATHu8sDrainNE
//  skips empty runs (so `a//b` is fine); we add the dotfile rule
//  (no `.`, `..`, `.git`) on top.  PATHu8sVerifySegment (called by
//  PATHu8bPush downstream) catches embedded `/`, NUL, \r, \n, \t.
static ok64 static_segs_ok(u8cs path) {
    sane(1);
    $eachseg(seg, path) {
        test(!$empty(seg) && $at(seg, 0) != '.', WOOFBADREQ);
    }
    done;
}

//  HTTP envelope for a static asset.  Caller supplies the
//  Content-Type as a NUL-term C string and the body length; we
//  render into a stack buffer and feed it into `out` (the conn's
//  ring buffer's idle range).
static ok64 static_envelope(Bu8 out, char const *mt, size_t content_len) {
    sane(1);
    a_pad(u8, hdr, 256);
    {
        a$str(s, "HTTP/1.1 200 OK\r\nContent-Type: ");
        call(u8bFeed, hdr, s);
    }
    {
        u8cs mts = { (u8c *)mt, (u8c *)mt + strlen(mt) };
        call(u8bFeed, hdr, mts);
    }
    {
        a$str(s, "\r\nContent-Length: ");
        call(u8bFeed, hdr, s);
    }
    {
        char num[32];
        int n = snprintf(num, sizeof(num), "%zu", content_len);
        u8cs ns = {(u8c *)num, (u8c *)num + n};
        call(u8bFeed, hdr, ns);
    }
    {
        a$str(s, "\r\nCache-Control: max-age=3600\r\nConnection: close\r\n\r\n");
        call(u8bFeed, hdr, s);
    }
    a_dup(u8c, hdr_s, u8bData(hdr));
    call(u8bFeed, out, hdr_s);
    done;
}

//  Serve `<repo>/.be/static/<rel>` to the conn.  Path is built segment-
//  by-segment via PATHu8bPush — every segment is re-validated by
//  abc/PATH (no '/', no NUL, no \r/\n/\t) and pre-screened for
//  dotfiles via static_segs_ok.  Feeds envelope + bytes into c->out,
//  flips state to WOOF_DRAIN.  No worker fork, no pipe registration.
//
//  Returns OK on hit; WOOFBADREQ on a dotfile / bad segment;
//  WOOFNOROUTE when the file isn't there.
static ok64 serve_static(conn *c, u8cs rel) {
    sane(c);
    call(static_segs_ok, rel);

    //  Build <root>/.be/static/<rel> — `a_path` feeds the base then
    //  Push-validates each subsequent segment.
    a_path(fpath);
    {
        a_dup(u8c, root, u8bDataC(WOOF.h->root));
        call(PATHu8bFeed, fpath, root);
    }
    a$str(dotbe,   ".be");
    a$str(staticd, "static");
    call(PATHu8bPush, fpath, dotbe);
    call(PATHu8bPush, fpath, staticd);
    $eachseg(seg, rel) {
        call(PATHu8bPush, fpath, seg);
    }

    //  mmap; on miss respond 404 (same arm as unknown route).
    u8bp mapped = NULL;
    if (FILEMapRO(&mapped, $path(fpath)) != OK) return WOOFNOROUTE;
    a_dup(u8c, body, u8bData(mapped));
    size_t blen = (size_t)$len(body);

    //  Last non-empty segment of `rel` is the basename → mime sniff.
    u8cs name = {};
    $eachseg(seg, rel) { $mv(name, seg); }

    Bu8 out = { c->out[0], c->out[1], c->out[2], c->out[3] };
    call(static_envelope, out, mime_for(name), blen);
    call(u8bFeed, out, body);
    c->out[2] = out[2];   //  commit advanced idle pointer

    (void)FILEUnMap(mapped);

    //  No worker, no pipe — flip straight to DRAIN so write_cb closes
    //  on empty ring instead of looping for more hunks.
    c->state = WOOF_DRAIN;
    done;
}

static void fail_reply(conn *c, char const *body, size_t n) {
    int fd = c->sock_fd;
    c->sock_fd = -1;
    send_and_close(fd, body, n);
    slot_release(c);
}

// --- helpers ---

static ok64 html_escape_into(Bu8 out, u8csc from) {
    sane(1);
    $for(u8c, p, from) {
        switch (*p) {
            case '<':  call(u8bFeed, out, LIT("&lt;"));   break;
            case '>':  call(u8bFeed, out, LIT("&gt;"));   break;
            case '&':  call(u8bFeed, out, LIT("&amp;"));  break;
            case '"':  call(u8bFeed, out, LIT("&quot;")); break;
            default:   call(u8bFeed1, out, *p);
        }
    }
    done;
}

//  Strip leading '/' from `target`; percent-decode the rest into
//  `out`.  Mirrors the test-scaffold in woof/test/HTTP.c.
static ok64 extract_be_uri(Bu8 out, u8cs target) {
    sane(1);
    u8bReset(out);
    test(!$empty(target) && *target[0] == '/', WOOFBADREQ);
    u8csUsed1(target);
    while (!$empty(target)) {
        u8 c = *target[0];
        if (c == '%') {
            test($len(target) >= 3, WOOFBADREQ);
            u8 hi = BASE16rev[target[0][1]];
            u8 lo = BASE16rev[target[0][2]];
            test(hi != 0xff && lo != 0xff, WOOFBADREQ);
            call(u8bFeed1, out, (u8)((hi << 4) | lo));
            u8csUsed(target, 3);
        } else {
            call(u8bFeed1, out, c);
            u8csUsed1(target);
        }
    }
    done;
}

//  fork+exec the worker with `--tlv` and the be-URI on argv.
//  Caller owns *rfd (close on conn teardown) and *pid (reap via
//  SIGCHLD path).
static ok64 spawn_worker(woof_route const *route, u8cs be_uri,
                         int *rfd, int *pid_out) {
    sane(route && rfd && pid_out);
    a_path(bin_path);
    {
        u8cs av0 = {};
        if (WOOF_ARGV0 != NULL) {
            av0[0] = (u8c *)WOOF_ARGV0;
            av0[1] = (u8c *)WOOF_ARGV0 + strlen(WOOF_ARGV0);
        }
        call(HOMEResolveSibling, WOOF.h, bin_path, route->binary, av0);
    }
    //  --tlv: workers emit nested TLV records (`dog/HUNK.h`).  woof
    //  drains them in pipe_cb and renders via HUNKu8sFeedHtml.  The
    //  per-conn pipe_in is sized (WOOF_PIPEBUF_BYTES) to hold any
    //  single record the worker bundles.
    a$str(tlv_flag, "--tlv");
    u8cs argv_a[3] = {
        { route->binary[0], route->binary[1] },
        { tlv_flag[0],      tlv_flag[1]      },
        { be_uri[0],        be_uri[1]        },
    };
    u8css argv = { argv_a, argv_a + 3 };
    pid_t pid = 0;
    call(FILESpawn, $path(bin_path), argv, NULL, rfd, &pid);
    *pid_out = (int)pid;
    //  Worker pipe non-blocking too — same reasoning.
    (void)fcntl(*rfd, F_SETFL, fcntl(*rfd, F_GETFL, 0) | O_NONBLOCK);
    done;
}

// --- forward decls ---

static short read_cb (int fd, poller *p);
static short pipe_cb (int fd, poller *p);
static short write_cb(int fd, poller *p);

// --- read_cb ---

static short read_cb(int fd, poller *p) {
    conn *c = (conn *)p->payload;
    if (c == NULL || c->sock_fd != fd) return 0;

    size_t room = (size_t)(c->in[3] - c->in[2]);
    if (room == 0) {
        fail_reply(c, RESP_400, sizeof(RESP_400) - 1);
        return 0;
    }
    ssize_t r = read(fd, c->in[2], room);
    if (r == 0) { slot_release(c); return 0; }
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return POLLIN;
        slot_release(c); return 0;
    }
    c->in[2] += r;
    c->last_io_ns = POLNow();

    HTTPstate *req = &c->req;
    zerop(req);
    //  TODO: hoist hdrs buffer into the conn slot — for now this
    //  re-allocates a 32-entry stack buf on every read.  Fine until
    //  a parse spans multiple read_cb invocations (which forces
    //  re-parsing the full prefix every tick).
    aBpad2(u8cs, hdrs, 32);
    req->headers = hdrsidle;
    u8cs view = { c->in[1], c->in[2] };
    ok64 hr = HTTPutf8Drain(view, req);
    if (hr == HTTPNONE) return POLLIN;
    if (hr != OK) { fail_reply(c, RESP_400, sizeof(RESP_400)-1); return 0; }

    //  method gate
    {
        a$str(get_s,  "GET");
        a$str(head_s, "HEAD");
        b8 ok = $eq(req->method, get_s) || $eq(req->method, head_s);
        if (!ok) { fail_reply(c, RESP_405, sizeof(RESP_405)-1); return 0; }
    }

    //  Stash the decoded be-URI in c->in's idle tail.  The bytes
    //  live as long as the slot.
    Bu8 beuri_buf = { c->in[2], c->in[2], c->in[2], c->in[3] };
    if (extract_be_uri(beuri_buf, req->uri) != OK) {
        fail_reply(c, RESP_400, sizeof(RESP_400)-1); return 0;
    }
    c->in[2] = beuri_buf[2];
    c->uri[0] = beuri_buf[1];
    c->uri[1] = beuri_buf[2];

    //  Static assets short-circuit: any path whose first segment is
    //  "static" is served from `<root>/.be/static/<rest>` with no
    //  worker fork.  `cur` advances past the consumed segment, so
    //  whatever PATHu8sDrainNE leaves behind IS the relative tail.
    {
        u8cs cur   = { c->uri[0], c->uri[1] };
        u8cs first = {};
        a$str(s_static, "static");
        if (PATHu8sDrainNE(cur, first) == OK && $eq(first, s_static)) {
            ok64 sr = serve_static(c, cur);
            if (sr == WOOFBADREQ) {
                fail_reply(c, RESP_400, sizeof(RESP_400)-1); return 0;
            }
            if (sr == WOOFNOROUTE) {
                fail_reply(c, RESP_404, sizeof(RESP_404)-1); return 0;
            }
            //  serve_static populated c->out and flipped to DRAIN.
            //  Swap our callback to write_cb and return POLLOUT —
            //  same shape as the worker-spawn path.
            p->callback = write_cb;
            return POLLOUT;
        }
    }

    //  Scheme → worker.
    uri u = {};
    a_dup(u8c, view2, c->uri);
    if (URIutf8Drain(view2, &u) != OK) {
        fail_reply(c, RESP_400, sizeof(RESP_400)-1); return 0;
    }
    woof_route const *route = WOOFRouteFind(u.scheme);
    if (route == NULL) {
        fail_reply(c, RESP_404, sizeof(RESP_404)-1); return 0;
    }

    //  Spawn worker → c->pipe_fd, c->worker_pid.
    if (spawn_worker(route, c->uri, &c->pipe_fd, &c->worker_pid) != OK) {
        fail_reply(c, RESP_500, sizeof(RESP_500)-1); return 0;
    }

    //  HTTP envelope + HTML prelude into c->out.
    (void)u8bFeed(c->out, LIT(RESP_200_HEAD));
    (void)u8bFeed(c->out, LIT(HTML_PRELUDE_HEAD));
    (void)html_escape_into(c->out, c->uri);
    (void)u8bFeed(c->out, LIT(HTML_PRELUDE_TAIL));

    c->state = WOOF_STREAM;

    //  Re-purpose THIS entry: callback becomes write_cb, mask becomes
    //  POLLOUT.  Mutate in-place — POLTrackEvents on the same fd would
    //  re-sift the heap and invalidate POLLoop's `at` pointer for the
    //  remainder of this iteration.
    p->callback = write_cb;

    //  Worker pipe is a NEW fd — safe to insert; push appends and
    //  sifts up only against equal-or-later deadlines, so our entry
    //  doesn't move.
    poller pp = { .callback = pipe_cb, .events = POLLIN,
                  .tofd = c->pipe_fd, .payload = c };
    (void)POLTrackEvents(c->pipe_fd, pp);

    return POLLOUT;
}

// --- pipe_cb ---

static short pipe_cb(int fd, poller *p) {
    conn *c = (conn *)p->payload;
    if (c == NULL || c->pipe_fd != fd) return 0;

    //  drain pipe → pipe_in idle tail
    size_t room = (size_t)(c->pipe_in[3] - c->pipe_in[2]);
    if (room > 0) {
        ssize_t r = read(fd, c->pipe_in[2], room);
        if (r > 0) c->pipe_in[2] += r;
        else if (r == 0) {
            //  worker EOF → drain remaining TLV, append postlude,
            //  flip to DRAIN.  Self-eject by returning 0; do NOT call
            //  POLIgnoreEvents on our own fd inside the callback —
            //  POLLoop ejects us based on the return value, and any
            //  mid-iteration heap mutation invalidates its `at`.
            //  TODO: drain trailing complete frames before postlude.
            (void)u8bFeed(c->out, LIT(HTML_POSTLUDE));
            c->state = WOOF_DRAIN;
            (void)close(c->pipe_fd);
            c->pipe_fd = -1;
            (void)POLAddEvents(c->sock_fd, POLLOUT);
            return 0;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                return POLLIN;
            slot_release(c); return 0;
        }
    }

    //  Drain TLV records while c->out has headroom AND pipe_in holds
    //  at least one complete frame.  HUNKu8sDrain advances `frame[0]`
    //  past the consumed bytes; we mirror that on pipe_in's data head.
    //  Short frames (HUNKu8sDrain != OK) wait for more bytes via the
    //  next POLLIN.
    while ((size_t)(c->out[3] - c->out[2]) >= WOOF_HUNK_HEADROOM
           && c->pipe_in[1] < c->pipe_in[2]) {
        hunk hk = {};
        u8cs frame = { c->pipe_in[1], c->pipe_in[2] };
        if (HUNKu8sDrain(frame, &hk) != OK) break;
        c->pipe_in[1] = (u8 *)frame[0];
        //  HUNKu8sFeedHtml writes into a `u8s` (head/term slice), not a
        //  `Bu8` (past/data/idle/term).  Hand it the idle range and
        //  commit the advanced head back into c->out's idle pointer.
        u8s idle = { c->out[2], c->out[3] };
        (void)HUNKu8sFeedHtml(idle, &hk);
        c->out[2] = idle[0];
    }
    //  Fully drained: reset pipe_in so the next read starts at base
    //  (cheap compaction; avoids fragmentation as records arrive).
    if (c->pipe_in[1] == c->pipe_in[2]) {
        u8 *base = WOOF.pool + (size_t)c->slot * WOOF_SLOT_BYTES
                 + WOOF_REQBUF_BYTES;
        c->pipe_in[0] = base; c->pipe_in[1] = base; c->pipe_in[2] = base;
    }

    c->last_io_ns = POLNow();
    if (c->out[1] != c->out[2]) (void)POLAddEvents(c->sock_fd, POLLOUT);
    //  Keep our entry registered (POLLIN) even when c->out lacks
    //  headroom — returning 0 ejects forever.  This is a minor busy-
    //  loop: poll() keeps reporting POLLIN, we drain into pipe_in,
    //  fail the headroom test, return.  Eventually write_cb frees
    //  headroom and POLAddEvents fans us in.  TODO: park properly.
    return POLLIN;
}

// --- write_cb ---

static short write_cb(int fd, poller *p) {
    conn *c = (conn *)p->payload;
    if (c == NULL || c->sock_fd != fd) return 0;

    size_t pending = (size_t)(c->out[2] - c->out[1]);
    while (pending > 0) {
        ssize_t w = write(fd, c->out[1], pending);
        if (w > 0) { c->out[1] += w; pending -= (size_t)w; continue; }
        if (errno == EAGAIN || errno == EWOULDBLOCK) return POLLOUT;
        if (errno == EINTR) continue;
        slot_release(c); return 0;
    }
    c->last_io_ns = POLNow();

    if (c->state == WOOF_DRAIN) {
        //  Pipe is gone, ring drained → done.  We're the sock's own
        //  callback, so skip POLIgnoreEvents on sock_fd; POLLoop
        //  ejects us when we return 0.
        slot_release_x(c, YES, fd);
        return 0;
    }

    //  Compact c->out so the pipe side can refill it from base.
    u8 *base = WOOF.pool + (size_t)c->slot * WOOF_SLOT_BYTES
             + WOOF_REQBUF_BYTES + WOOF_PIPEBUF_BYTES;
    c->out[0] = base; c->out[1] = base; c->out[2] = base;

    //  Wake the pipe side if it was paused for headroom.
    if (c->pipe_fd >= 0) (void)POLAddEvents(c->pipe_fd, POLLIN);
    //  Park: client doesn't write on its own (request done, awaiting
    //  body) so POLLIN won't fire until either pipe_cb does
    //  POLAddEvents(POLLOUT) or the peer closes early (POLLIN with
    //  read==0).  TODO: handle peer-close by detecting POLLHUP via
    //  p->revents on entry.
    return POLLIN;
}

// --- public entry points ---

ok64 WOOFConnAccept(int cfd) {
    sane(cfd >= 0);
    //  Non-blocking on every conn fd: POLLoop's deadline-driven
    //  timeout pass can fire callbacks even when poll() didn't see
    //  I/O ready (see WOOFOpen for the matching listen-fd note).
    (void)fcntl(cfd, F_SETFL, fcntl(cfd, F_GETFL, 0) | O_NONBLOCK);
    conn *c = slot_claim(cfd);
    if (c == NULL) return WOOFBUSY;
    poller pol = {
        .callback = read_cb,
        .events   = POLLIN,
        .tofd     = cfd,
        .payload  = c,
    };
    return POLTrackEvents(cfd, pol);
}

void WOOFConnCloseAll(void) {
    if (WOOF.pool == NULL) return;
    conn *cs = connbDataHead(WOOF.conns);
    for (u32 i = 0; i < WOOF.max_conns; i++) {
        if (cs[i].sock_fd >= 0 || cs[i].pipe_fd >= 0 || cs[i].worker_pid > 0) {
            slot_release(&cs[i]);
        }
    }
}

void WOOFConnReapAll(void) {
    while (1) {
        int status = 0;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0) return;
        //  Clear pid on the owning conn so slot_release doesn't try to
        //  kill an already-reaped pid.
        conn *cs = connbDataHead(WOOF.conns);
        for (u32 i = 0; i < WOOF.max_conns; i++) {
            if (cs[i].worker_pid == (int)pid) {
                cs[i].worker_pid = -1;
                break;
            }
        }
    }
}

//  HUNKu8sFeedHtml lives in dog/HUNK.c alongside FeedColor / FeedText.
