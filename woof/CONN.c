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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE          // memfd_create (in-process projection capture)
#endif

#include "WOOF.h"

#include "abc/FILE.h"
#include "abc/HEX.h"
#include "abc/HTTP.h"
#include "abc/PRO.h"
#include "abc/TCP.h"
#include "abc/URI.h"
#include "dog/DOG.h"         // DOGProjectorDog (scheme → dog)
#include "dog/HOME.h"
#include "dog/HUNK.h"
#include "graf/GRAF.h"       // in-process projections (--api mode):
#include "keeper/KEEP.h"     //   graf (log/diff/...), keeper (blob/tree/
#include "spot/CAPO.h"       //   ...), spot (grep/...), sniff (ls/cat/...)
#include "sniff/SNIFF.h"
#include "sniff/AT.h"        // SNIFFAtTailOf — forward the wtlog tip

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
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
void WOOFConnCarve(conn *c) {
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
            WOOFConnCarve(c);
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
    a_dup(u8c, scan, name);
    if (u8csRevFind(scan, '.') != OK) return "application/octet-stream";
    //  RevFind left scan[1] one past the matched dot — the bytes from
    //  there to name's term are the extension.
    u8cs ext = { scan[1], name[1] };
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
//  WOOFNOROUTE when the file isn't there; BNOROOM when c->out lacks
//  room for the envelope+body (the mapping is still released).
//  Exported (WOOF.h) so test/fuzz drives the exact map/unmap path.
ok64 WOOFServeStatic(conn *c, u8cs rel) {
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

    //  From here on `mapped` owns a VMA + fd: every exit MUST unmap.
    //  `static_envelope`/`u8bFeed` can return BNOROOM (ring idle <
    //  body); a plain `call()` would `return` past FILEUnMap, leaking
    //  one file-sized mapping + fd per request (MEM-030).  `callsafe`
    //  runs the unmap on the failure arm before propagating.
    Bu8 out = { c->out[0], c->out[1], c->out[2], c->out[3] };
    callsafe(static_envelope(out, mime_for(name), blen),
             (void)FILEUnMap(mapped));
    callsafe(u8bFeed(out, body), (void)FILEUnMap(mapped));
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
//  `out`.  Exposed via WOOF.h so tests and woof/fuzz drive the exact
//  same decode read_cb does.
ok64 WOOFutf8ExtractURI(Bu8 out, u8cs target) {
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

// --- routing decision (shared with woof/fuzz) ---
//
//  Given c->uri (the decoded be-URI), run the non-fork part of
//  dispatch: a "static/…" first segment is served inline into c->out;
//  any known scheme yields its route; everything else is an error.
//  This is read_cb's parse path lifted out so the fuzz harness can
//  exercise it in-process without forking a worker.
woof_disp WOOFConnRoute(conn *c, woof_route const **route, ok64 *err) {
    *route = NULL;
    *err   = OK;

    //  Static assets short-circuit: first segment "static" → serve
    //  <root>/.be/static/<rest> with no worker fork.  `cur` advances
    //  past the consumed segment, so the tail IS the relative path.
    {
        u8cs cur   = { c->uri[0], c->uri[1] };
        u8cs first = {};
        a$str(s_static, "static");
        if (PATHu8sDrainNE(cur, first) == OK && $eq(first, s_static)) {
            ok64 sr = WOOFServeStatic(c, cur);
            if (sr == OK) return WOOF_DISP_STATIC;
            *err = (sr == WOOFBADREQ) ? WOOFBADREQ : WOOFNOROUTE;
            return WOOF_DISP_ERROR;
        }
    }

    //  Scheme → worker route.
    uri u = {};
    a_dup(u8c, view, c->uri);
    if (URIutf8Drain(view, &u) != OK) {
        *err = WOOFBADREQ;
        return WOOF_DISP_ERROR;
    }
    woof_route const *r = WOOFRouteFind(u.scheme);
    if (r == NULL) {
        *err = WOOFNOROUTE;
        return WOOF_DISP_ERROR;
    }
    *route = r;
    return WOOF_DISP_WORKER;
}

// --- TLV → HTML render (shared by worker pipe + in-process paths) ---

//  Drain complete TLV frames from c->pipe_in, rendering each as HTML
//  into c->out's idle range while ≥ WOOF_HUNK_HEADROOM idle bytes
//  remain.  Advances both data heads; short trailing frames wait.
//
//  Render one drained HUNK frame into c->out.  HUNKu8sDrain carves the
//  frame's TOK array as BASS scratch (hunk_drain_toks, dog/HUNK.c) that
//  must outlive the drain and survive into HUNKu8sFeedHtml — so the
//  drain+feed pair lives together in THIS helper, and WOOFRenderHunks
//  invokes it through call(), whose frame snapshots+rewinds BASS once
//  per hunk (the toks are consumed by HUNKu8sFeedHtml before that
//  rewind).  `*more` reports whether a full frame rendered: NO means no
//  complete frame remains — a normal stop, not an error.
static ok64 woof_render_one_hunk(conn *c, b8 *more) {
    sane(c && more);
    *more = NO;
    hunk hk = {};
    u8cs frame = { c->pipe_in[1], c->pipe_in[2] };
    if (HUNKu8sDrain(frame, &hk) != OK) done;   //  short frame: wait for more
    c->pipe_in[1] = (u8 *)frame[0];
    //  HUNKu8sFeedHtml writes a `u8s` (head/term) into c->out's idle
    //  range — a persistent conn buffer, not BASS, so it survives the
    //  call() rewind; commit the advanced head back.
    u8s idle = { c->out[2], c->out[3] };
    call(HUNKu8sFeedHtml, idle, &hk);
    c->out[2] = idle[0];
    *more = YES;
    done;
}

//  MEM-038: drain HUNK frames into the output buffer until c->out runs
//  low on headroom or the pipe holds no complete frame.  Each frame's
//  TOK carve is BASS scratch with no natural enclosing frame — POL
//  invokes woof's callbacks (pipe_cb / serve_inproc) bare — so we
//  reclaim it the idiomatic way: render every hunk through a call() to
//  woof_render_one_hunk, whose frame rewinds that scratch per hunk.  The
//  arena stays flat across hunks AND requests with no manual arena
//  poking, and a render error propagates instead of being swallowed.
ok64 WOOFRenderHunks(conn *c) {
    sane(c);
    while ((size_t)(c->out[3] - c->out[2]) >= WOOF_HUNK_HEADROOM
           && c->pipe_in[1] < c->pipe_in[2]) {
        b8 more = NO;
        call(woof_render_one_hunk, c, &more);
        if (!more) break;
    }
    done;
}

// --- in-process projection dispatch (`--api` mode) ---
//
//  Instead of fork+exec'ing `be --tlv <uri>`, run the owning dog's
//  Exec as a library call against singletons opened once at serve
//  start.  All four projector dogs are wired: keeper (sha1/blob/tree/
//  commit/refs/size/type), graf (log/diff/blame/weave/map), spot
//  (spot/grep/regex), sniff (ls/lsr/cat/status).  A dog that fails to
//  open (e.g. sniff with no worktree) leaves its schemes on the fork
//  path.  Single project — multi-project dispatch (a per-project dog
//  cache keyed off the URI's project) is a TODO.
//
//  keeper is the shared object store: woof opens it once and owns it;
//  graf/spot find it already open (KEEPOPEN) and never close it, so
//  the four singletons coexist over one keeper.

static cli  woof_api_cli   = {};
static int  woof_api_memfd = -1;
static b8   woof_api_ready = NO;
static b8   woof_api_keep_owned = NO;
static b8   woof_api_have_keep  = NO;
static b8   woof_api_have_graf  = NO;
static b8   woof_api_have_spot  = NO;
static b8   woof_api_have_sniff = NO;
static char const *woof_api_dog = NULL;   //  selects the Exec in run_capture

ok64 WOOFApiOpen(void) {
    sane(1);
    if (woof_api_ready) return OK;
    if (WOOF.h == NULL) fail(WOOFFAIL);

    //  Forward the worktree tip into the home, exactly as `be` resolves
    //  `--at <root>?<branch>#<sha>` for the fork path.  Without it,
    //  tip-scoped projections (blame, log:<file>, wt-diff) run unscoped;
    //  with it they match fork.  SNIFFAtTailOf peeks `<wt>/.be/wtlog`
    //  RO (no keeper).  Missing/no-tip ⇒ leave cur_sha empty (unscoped,
    //  as before).
    {
        a_pad(u8, at_buf, FILE_PATH_MAX_LEN + 128);
        a_dup(u8c, wt, u8bDataC(WOOF.h->wt));
        if (SNIFFAtTailOf(wt, at_buf) == OK) {
            uri at = {};
            u8csMv(at.data, u8bDataC(at_buf));
            URILexer(&at);
            if (u8csLen(at.fragment) == 40) {
                u8bReset(WOOF.h->cur_sha);
                (void)u8bFeed(WOOF.h->cur_sha, at.fragment);
            }
            //  Tail query is `?/<project>/<branch>`; strip the project
            //  and set cur_branch (empty == trunk) so opens + ref
            //  resolution land on the wt's branch.
            if (!u8csEmpty(at.query)) {
                a_dup(u8c, q, at.query);
                DOGQueryStripProject(q);
                (void)HOMESetCurBranch(WOOF.h, q);
            }
        }
    }

    a_dup(u8c, br, u8bDataC(WOOF.h->cur_branch));

    //  Keeper first — the shared object store every projector reads
    //  through.  woof owns it; KEEPOPEN means someone else already did.
    {
        ok64 ko = KEEPOpenBranch(WOOF.h, br, NO);
        if (ko != OK && ko != KEEPOPEN) return ko;
        woof_api_have_keep  = YES;
        woof_api_keep_owned = (ko == OK);
    }
    //  Best-effort per dog: graf/spot find keeper open (won't close it);
    //  sniff serves the worktree.  Flat store ⇒ one open serves every
    //  branch (per-request `?ref` resolves via REFS, no reopen).
    {
        ok64 go = GRAFOpenBranch(WOOF.h, br, NO);
        woof_api_have_graf = (go == OK || go == GRAFOPEN);
    }
    {
        ok64 so = SPOTOpenBranch(WOOF.h, br, NO);
        woof_api_have_spot = (so == OK || so == SPOTOPEN);
    }
    woof_api_have_sniff = (SNIFFOpen(WOOF.h, NO) == OK);

    call(PATHu8bAlloc, woof_api_cli.repo);
    call(u8csbAlloc,   woof_api_cli.flags, CLI_MAX_FLAGS * 2);
    call(u8csbAlloc,   woof_api_cli.uris,  CLI_MAX_URIS);
    //  cli repo = the WORKTREE root (where tracked files live), matching
    //  the cwd graf inherits in fork mode.  graf blame / wt-diff / weave
    //  read the wt file from here; objects still come from the store via
    //  the open keeper.  Differs from h->root for a secondary worktree.
    {
        a_dup(u8c, wt, u8bDataC(WOOF.h->wt));
        if (u8csEmpty(wt)) u8csMv(wt, u8bDataC(WOOF.h->root));
        call(PATHu8bFeed, woof_api_cli.repo, wt);
    }

#if defined(__linux__)
    woof_api_memfd = memfd_create("woof-api", 0);
#else
    //  Portable anon-fd: mkstemp + immediate unlink.  No on-disk name
    //  outlives the open fd; ftruncate(0) on every request keeps it
    //  scratch-only.  macOS / FreeBSD lack memfd_create.
    char tmpl[] = "/tmp/woof-api.XXXXXX";
    woof_api_memfd = mkstemp(tmpl);
    if (woof_api_memfd >= 0) (void)unlink(tmpl);
#endif
    if (woof_api_memfd < 0) fail(WOOFFAIL);

    woof_api_ready = YES;
    done;
}

void WOOFApiClose(void) {
    if (!woof_api_ready) return;
    //  Close in reverse open order; keeper last (woof owns it — graf /
    //  spot opened it as KEEPOPEN and left it for us).
    if (woof_api_have_sniff) SNIFFClose();
    if (woof_api_have_spot)  SPOTClose();
    if (woof_api_have_graf)  (void)GRAFClose();
    if (woof_api_keep_owned) (void)KEEPClose();
    if (woof_api_memfd >= 0) { (void)close(woof_api_memfd); woof_api_memfd = -1; }
    u8csbFree(woof_api_cli.uris);
    u8csbFree(woof_api_cli.flags);
    PATHu8bFree(woof_api_cli.repo);
    zerop(&woof_api_cli);
    woof_api_have_keep = woof_api_have_graf = NO;
    woof_api_have_spot = woof_api_have_sniff = NO;
    woof_api_keep_owned = NO;
    woof_api_ready = NO;
}

//  YES iff the dog that owns this scheme was opened in-process.
b8 WOOFApiDogOpen(char const *dog) {
    if (strcmp(dog, "keeper") == 0) return woof_api_have_keep;
    if (strcmp(dog, "graf")   == 0) return woof_api_have_graf;
    if (strcmp(dog, "spot")   == 0) return woof_api_have_spot;
    if (strcmp(dog, "sniff")  == 0) return woof_api_have_sniff;
    return NO;
}

//  The capture fd holding the most recent WOOFApiRun output (TLV).
//  Exposed so the fuzz harness can drain it; read after WOOFApiRun.
int WOOFApiMemfd(void) { return woof_api_memfd; }

//  Run the selected dog's Exec with stdout captured into the memfd.
//  Reached via `try` from serve_inproc so call/try snapshot+rewind
//  ABC_BASS — the dogs' per-call carves don't accumulate across
//  requests (the trap the fuzzer hit).
static ok64 run_capture(void) {
    sane(1);
    GRAFArenaCleanup();   //  reset graf's render arena; harmless for others
    int save = dup(STDOUT_FILENO);
    (void)ftruncate(woof_api_memfd, 0);
    (void)lseek(woof_api_memfd, 0, SEEK_SET);
    (void)dup2(woof_api_memfd, STDOUT_FILENO);
    if      (strcmp(woof_api_dog, "keeper") == 0) (void)KEEPExec(&woof_api_cli);
    else if (strcmp(woof_api_dog, "graf")   == 0) (void)GRAFExec(&woof_api_cli);
    else if (strcmp(woof_api_dog, "spot")   == 0) (void)SPOTExec(&woof_api_cli);
    else if (strcmp(woof_api_dog, "sniff")  == 0) (void)SNIFFExec(&woof_api_cli);
    (void)dup2(save, STDOUT_FILENO);
    (void)close(save);
    done;
}

//  Run `dog`'s Exec on the verbless projector URI `u`, capturing its
//  TLV into the shared memfd (WOOFApiMemfd).  No conn, no render — the
//  reusable dispatch core shared by serve_inproc (server) and the fuzz
//  harness, so both exercise the identical in-process path.  Caller
//  must have WOOFApiDogOpen(dog).
ok64 WOOFApiRun(uri *u, char const *dog) {
    sane(u && dog);
    if (!woof_api_ready) fail(WOOFFAIL);

    //  Verbless projector cli — each dog synthesizes the verb from the
    //  scheme.  Store the raw URI text (URI-004); the dog's Exec parses
    //  it on demand.  `u->data` must stay alive for the call.
    u8csbReset(woof_api_cli.uris);
    call(u8csbFeed1, woof_api_cli.uris, u->data);
    zerop(&woof_api_cli.verb);
    HUNKMode = HUNKOutTLV;
    woof_api_dog = dog;

    try(run_capture);   //  BASS rewound here; projection errors ignored
    __ = OK;
    done;
}

//  Serve a projector in-process: WOOFApiRun → TLV (memfd) → c->pipe_in →
//  HTML in c->out (same render the worker path uses) → DRAIN.  No fork.
//  OK on success; non-OK lets read_cb fork instead.
static ok64 serve_inproc(conn *c, uri *u, char const *dog) {
    sane(c && u && dog);
    call(WOOFApiRun, u, dog);

    //  TLV → c->pipe_in (the buffer the worker path fills), capped.
    (void)lseek(woof_api_memfd, 0, SEEK_SET);
    {
        size_t room = (size_t)(c->pipe_in[3] - c->pipe_in[2]);
        if (room > 0) {
            ssize_t n = read(woof_api_memfd, c->pipe_in[2], room);
            if (n > 0) c->pipe_in[2] += n;
        }
    }

    //  HTTP envelope + HTML prelude, render the TLV, then postlude.
    Bu8 out = { c->out[0], c->out[1], c->out[2], c->out[3] };
    call(u8bFeed, out, LIT(RESP_200_HEAD));
    call(u8bFeed, out, LIT(HTML_PRELUDE_HEAD));
    call(html_escape_into, out, c->uri);
    call(u8bFeed, out, LIT(HTML_PRELUDE_TAIL));
    c->out[2] = out[2];

    call(WOOFRenderHunks, c);

    {
        Bu8 tail = { c->out[0], c->out[1], c->out[2], c->out[3] };
        call(u8bFeed, tail, LIT(HTML_POSTLUDE));
        c->out[2] = tail[2];
    }
    c->state = WOOF_DRAIN;
    done;
}

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
    if (WOOFutf8ExtractURI(beuri_buf, req->uri) != OK) {
        fail_reply(c, RESP_400, sizeof(RESP_400)-1); return 0;
    }
    c->in[2] = beuri_buf[2];
    c->uri[0] = beuri_buf[1];
    c->uri[1] = beuri_buf[2];

    //  Decide: static asset (served inline), worker route, or error.
    //  WOOFConnRoute runs the same parse path woof/fuzz drives; only
    //  the worker fork + HTML envelope below are read_cb's own.
    woof_route const *route = NULL;
    ok64 rerr = OK;
    switch (WOOFConnRoute(c, &route, &rerr)) {
        case WOOF_DISP_ERROR:
            if (rerr == WOOFBADREQ)
                fail_reply(c, RESP_400, sizeof(RESP_400) - 1);
            else
                fail_reply(c, RESP_404, sizeof(RESP_404) - 1);
            return 0;
        case WOOF_DISP_STATIC:
            //  serve_static populated c->out and flipped to DRAIN.
            //  Swap our callback to write_cb and return POLLOUT —
            //  same shape as the worker-spawn path.
            p->callback = write_cb;
            return POLLOUT;
        case WOOF_DISP_WORKER:
            break;
    }

    //  --api: dispatch in-process to whichever projector dog owns the
    //  scheme and is open; otherwise fall through to the fork path.
    if (WOOF.api) {
        uri au = {};
        a_dup(u8c, av, c->uri);
        if (URIutf8Drain(av, &au) == OK) {
            //  Restore data to the full URI — URIutf8Drain consumes it,
            //  but downstream dogs expect the canonical slice CLIParse
            //  hands them (GRAFResolveTip rebuilds the pre-query text
            //  from data[0]..query[0]; a consumed data[0] makes that
            //  slice negative-length → memcpy abort).
            au.data[0] = c->uri[0];
            au.data[1] = c->uri[1];
            char const *dog = DOGProjectorDog(au.scheme);
            if (dog != NULL && WOOFApiDogOpen(dog)
                && serve_inproc(c, &au, dog) == OK) {
                p->callback = write_cb;
                return POLLOUT;
            }
        }
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
    //  pipe_cb is a bare POL callback (returns short, no call() frame), so
    //  render best-effort: WOOFRenderHunks reclaims its own per-hunk BASS
    //  scratch internally, and there's no ok64 channel to propagate on.
    (void)WOOFRenderHunks(c);
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
