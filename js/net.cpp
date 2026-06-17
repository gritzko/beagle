#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "JABC.hpp"
extern "C" {
#include "abc/NET.h"
#include "abc/TCP.h"
#include "abc/UDP.h"
}

//  net/dgram + timers — a Node-style async API built on `pol`.  The native side
//  is leaf-only (socket syscalls returning bare fds, like io.*); the per-socket
//  buffers, the EventEmitter dispatch, and the setTimeout/setInterval timer
//  wheel all live in the embedded JS bundle.  Sockets register their fd with
//  pol.watch; readiness drives 'data'/'drain'/'connection'; nothing here holds
//  memory or a JS reference.  Address form is a `tcp://host:port` URI (parsed by
//  abc/NET); EAGAIN on a nonblocking socket surfaces to JS as -1, never a throw.

static int JABCInt(JSContextRef ctx, JSValueRef v, JSValueRef* e) {
  return (int)JSValueToNumber(ctx, v, e);
}

//  Mark an fd nonblocking so the pol loop never wedges on a slow peer.
static void JABCNonblock(int fd) {
  int fl = fcntl(fd, F_GETFL, 0);
  if (fl != -1) (void)fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

//  Copy a JS-string URI argument into `buf`; return its byte length (no NUL).
static size_t JABCUri(char* buf, size_t cap, JSContextRef ctx, JSValueRef arg) {
  if (!JSValueIsString(ctx, arg)) return 0;
  JSStringRef s = JSValueToStringCopy(ctx, arg, NULL);
  size_t n = JSStringGetUTF8CString(s, buf, cap);  //  counts the NUL
  JSStringRelease(s);
  return n ? n - 1 : 0;
}

//  net._listen(uri) -> fd  (TCP bind+listen, nonblocking)
static JABC_FN(JABCNetListen) {
  if (argc < 1) JABC_THROW("net._listen(uri)");
  char uri[256];
  size_t n = JABCUri(uri, sizeof(uri), ctx, args[0]);
  if (n == 0) JABC_THROW("net._listen: bad uri");
  u8cs addr = {(u8*)uri, (u8*)uri + n};
  int fd = -1;
  if (TCPListen(&fd, addr) != OK || fd < 0) JABC_THROW(strerror(errno));
  JABCNonblock(fd);
  return JSValueMakeNumber(ctx, (double)fd);
}

//  net._connect(uri) -> fd  (TCP connect; abc connect is synchronous, then we
//  flip the fd nonblocking — v1 limitation, see POL.md)
static JABC_FN(JABCNetConnect) {
  if (argc < 1) JABC_THROW("net._connect(uri)");
  char uri[256];
  size_t n = JABCUri(uri, sizeof(uri), ctx, args[0]);
  if (n == 0) JABC_THROW("net._connect: bad uri");
  u8csc addr = {(u8*)uri, (u8*)uri + n};
  int fd = -1;
  if (TCPConnect(&fd, addr, YES) != OK || fd < 0) JABC_THROW(strerror(errno));
  JABCNonblock(fd);
  return JSValueMakeNumber(ctx, (double)fd);
}

//  net._accept(sfd) -> cfd, or -1 if nothing pending (EAGAIN/spurious)
static JABC_FN(JABCNetAccept) {
  if (argc < 1) JABC_THROW("net._accept(sfd)");
  int sfd = JABCInt(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  int cfd = -1;
  aNETraw(caddr);
  if (TCPAccept(&cfd, caddr, sfd) != OK || cfd < 0)
    return JSValueMakeNumber(ctx, -1);  //  no connection ready right now
  JABCNonblock(cfd);
  return JSValueMakeNumber(ctx, (double)cfd);
}

//  net._recv(fd, u8) -> n  (n>0 bytes, 0 = EOF, -1 = would-block)
static JABC_FN(JABCNetRecv) {
  if (argc < 2) JABC_THROW("net._recv(fd, Uint8Array)");
  int fd = JABCInt(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  u8s ta = {};
  if (!JABCBytesOf(ta, ctx, args[1], exception)) return JSValueMakeUndefined(ctx);
  ssize_t n;
  do { n = recv(fd, ta[0], $len(ta), 0); } while (n < 0 && errno == EINTR);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) return JSValueMakeNumber(ctx, -1);
    JABC_THROW(strerror(errno));
  }
  return JSValueMakeNumber(ctx, (double)n);
}

//  net._send(fd, u8) -> n  (bytes written, or -1 = would-block).  MSG_NOSIGNAL
//  turns a write to a closed peer into EPIPE instead of SIGPIPE.
static JABC_FN(JABCNetSend) {
  if (argc < 2) JABC_THROW("net._send(fd, Uint8Array)");
  int fd = JABCInt(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  u8s ta = {};
  if (!JABCBytesOf(ta, ctx, args[1], exception)) return JSValueMakeUndefined(ctx);
  ssize_t n;
  do { n = send(fd, ta[0], $len(ta), MSG_NOSIGNAL); } while (n < 0 && errno == EINTR);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) return JSValueMakeNumber(ctx, -1);
    JABC_THROW(strerror(errno));
  }
  return JSValueMakeNumber(ctx, (double)n);
}

//  net._shutwr(fd) — half-close the write side (send FIN), keep reading.  This
//  is what socket.end() maps to; the fd is closed only once the peer FINs too.
static JABC_FN(JABCNetShutwr) {
  if (argc < 1) JABC_THROW("net._shutwr(fd)");
  int fd = JABCInt(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  (void)shutdown(fd, SHUT_WR);
  JABC_UNDEF;
}

//  net._close(fd)
static JABC_FN(JABCNetClose) {
  if (argc < 1) JABC_THROW("net._close(fd)");
  int fd = JABCInt(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  (void)close(fd);
  JABC_UNDEF;
}

//  --- dgram (UDP) leaves: connectionless, sender address surfaced per datagram

//  dgram._bind(uri) -> fd  (UDP bind, nonblocking)
static JABC_FN(JABCDgramBind) {
  if (argc < 1) JABC_THROW("dgram._bind(uri)");
  char uri[256];
  size_t n = JABCUri(uri, sizeof(uri), ctx, args[0]);
  if (n == 0) JABC_THROW("dgram._bind: bad uri");
  u8cs addr = {(u8*)uri, (u8*)uri + n};
  int fd = -1;
  if (UDPBind(&fd, addr) != OK || fd < 0) JABC_THROW(strerror(errno));
  JABCNonblock(fd);
  return JSValueMakeNumber(ctx, (double)fd);
}

static void JABCSetProp(JSContextRef ctx, JSObjectRef o, const char* k, JSValueRef v) {
  JSStringRef s = JSStringCreateWithUTF8CString(k);
  JSObjectSetProperty(ctx, o, s, v, kJSPropertyAttributeNone, NULL);
  JSStringRelease(s);
}

//  dgram._recv(fd, u8) -> {n, address, port}, or null if nothing pending
static JABC_FN(JABCDgramRecv) {
  if (argc < 2) JABC_THROW("dgram._recv(fd, Uint8Array)");
  int fd = JABCInt(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  u8s ta = {};
  if (!JABCBytesOf(ta, ctx, args[1], exception)) return JSValueMakeUndefined(ctx);
  struct sockaddr_storage ss;
  socklen_t sl = sizeof(ss);
  ssize_t n;
  do { n = recvfrom(fd, ta[0], $len(ta), 0, (struct sockaddr*)&ss, &sl); }
  while (n < 0 && errno == EINTR);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) return JSValueMakeNull(ctx);
    JABC_THROW(strerror(errno));
  }
  char host[NI_MAXHOST] = "", serv[NI_MAXSERV] = "";
  getnameinfo((struct sockaddr*)&ss, sl, host, sizeof(host), serv, sizeof(serv),
              NI_NUMERICHOST | NI_NUMERICSERV);
  JSObjectRef o = JSObjectMake(ctx, NULL, NULL);
  JABCSetProp(ctx, o, "n", JSValueMakeNumber(ctx, (double)n));
  JABCSetProp(ctx, o, "address", JSOfCString(host));
  JABCSetProp(ctx, o, "port", JSValueMakeNumber(ctx, (double)atoi(serv)));
  return o;
}

//  dgram._send(fd, u8, host, port) -> n  (resolve + sendto), or -1 = would-block
static JABC_FN(JABCDgramSend) {
  if (argc < 4) JABC_THROW("dgram._send(fd, u8, host, port)");
  int fd = JABCInt(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  u8s ta = {};
  if (!JABCBytesOf(ta, ctx, args[1], exception)) return JSValueMakeUndefined(ctx);
  char host[NETmaxhost] = "", port[NETmaxserv] = "";
  if (JABCUri(host, sizeof(host), ctx, args[2]) == 0) JABC_THROW("dgram._send: bad host");
  int p = JABCInt(ctx, args[3], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  snprintf(port, sizeof(port), "%d", p);
  struct addrinfo hints = {}, *res = NULL;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  if (getaddrinfo(host, port, &hints, &res) != 0 || res == NULL)
    JABC_THROW("dgram._send: resolve failed");
  ssize_t n;
  do { n = sendto(fd, ta[0], $len(ta), 0, res->ai_addr, res->ai_addrlen); }
  while (n < 0 && errno == EINTR);
  freeaddrinfo(res);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) return JSValueMakeNumber(ctx, -1);
    JABC_THROW(strerror(errno));
  }
  return JSValueMakeNumber(ctx, (double)n);
}

//  The EventEmitter, net.createServer/connect + Socket, dgram, and the Node
//  timer wheel (setTimeout/setInterval over the single pol timer) all live in JS.
static const char* JABC_NET_JS = R"JS(
(function (g) {
  "use strict";
  const pol = g.pol, io = g.io, utf8 = g.utf8, Buf = g.Buf, net = g.net, dgram = g.dgram;

  // --- minimal Node EventEmitter ---
  class Emitter {
    constructor() { this._ev = {}; }
    on(name, fn) { (this._ev[name] = this._ev[name] || []).push(fn); return this; }
    once(name, fn) { const w = (...a) => { this.off(name, w); fn.apply(null, a); }; return this.on(name, w); }
    off(name, fn) { const a = this._ev[name]; if (a) { const i = a.indexOf(fn); if (i >= 0) a.splice(i, 1); } return this; }
    emit(name) { const a = this._ev[name]; if (!a) return false;
      const args = Array.prototype.slice.call(arguments, 1);
      for (const fn of a.slice()) fn.apply(null, args); return true; }
  }
  g.Emitter = Emitter;

  // --- Socket: a fd + a read/write Buf, registered with pol ---
  class Socket extends Emitter {
    constructor(fd) {
      super();
      this.fd = fd;
      this._rb = io.buf(64 << 10);     // read buffer (recycled per 'data' tick)
      this._wb = io.buf(64 << 10);     // write FIFO
      this._readEnded = false;     // peer FIN seen (recv 0)
      this._ending = false;        // end() called: finish writing then shutwr
      this._wrShut = false;        // our FIN sent (shutdown SHUT_WR)
      this._closed = false;
    }
    _register() { pol.watch(this.fd, pol.IN, (fd, rev) => this._poll(rev)); return this; }

    _poll(revents) {
      if (revents & (pol.ERR | pol.NVAL)) { this.emit("error", "socket error"); this._doClose(); return 0; }
      if (revents & pol.OUT) this._flush();
      if (revents & pol.IN) {
        this._rb.reset();
        const n = net._recv(this.fd, this._rb.idle());
        if (n > 0) { this._rb.fed(n); this.emit("data", this._rb.data()); }
        else if (n === 0) this._readEnd();   // EOF (n<0 = EAGAIN: nothing)
      }
      if ((revents & pol.HUP) && !(revents & pol.IN)) this._readEnd();
      return this._mask();
    }
    //  peer closed its write side: surface 'end', then finish our own write side
    //  (Node default, no allowHalfOpen) so the fd eventually closes.
    _readEnd() { if (this._readEnded) return; this._readEnded = true; this.emit("end"); this.end(); }
    _flush() {
      while (this._wb.size > 0) {
        const n = net._send(this.fd, this._wb.data());
        if (n < 0) break;                    // would-block: try again next tick
        this._wb.skip(n);                    // consume what went out
      }
      if (this._wb.size === 0) {
        this._wb.reset();
        this.emit("drain");
        if (this._ending && !this._wrShut) { this._wrShut = true; net._shutwr(this.fd); }
      }
    }
    _mask() {
      if (this._closed) return 0;
      if (this._readEnded && this._wrShut) { this._doClose(); return 0; }  // both sides done
      let m = 0;
      if (!this._readEnded) m |= pol.IN;                          // still reading
      if (this._wb.size > 0 || (this._ending && !this._wrShut)) m |= pol.OUT;  // flush / FIN pending
      return m || pol.IN;
    }
    _doClose() { if (this._closed) return; this._closed = true; net._close(this.fd); this.emit("close"); }

    write(data) {
      if (this._closed || this._ending) return false;
      const u = (typeof data === "string") ? utf8.Encode(data)
              : (data instanceof Buf) ? data.data() : data;
      if (u.length > this._wb.room) {        // grow the FIFO if backed up
        this._wb.shift();
        if (u.length > this._wb.room) this._wb.grow(this._wb.size + u.length + (64 << 10));
      }
      this._wb.feed(u);
      pol.more(this.fd, pol.OUT);            // ask the loop to drain it
      return this._wb.size < (256 << 10);    // false = backpressure (Node hint)
    }
    end(data) {
      if (data != null) this.write(data);
      this._ending = true;
      if (!this._closed) { try { pol.more(this.fd, pol.OUT); } catch (e) {} }  // force a close tick
      return this;
    }
    destroy() { this._ending = true; this._doClose(); return this; }
  }
  g.Socket = Socket;

  // --- net.createServer / connect ---
  class Server extends Emitter {
    constructor() { super(); this.fd = -1; }
    listen(port, host, cb) {
      if (typeof host === "function") { cb = host; host = undefined; }
      host = host || "0.0.0.0";
      this.fd = net._listen("tcp://" + host + ":" + port);
      if (cb) this.on("listening", cb);
      pol.watch(this.fd, pol.IN, (fd) => {
        for (;;) {
          const cfd = net._accept(fd);
          if (cfd < 0) break;                // drained the backlog
          this.emit("connection", new Socket(cfd)._register());
        }
        return pol.IN;
      });
      g.setTimeout(() => this.emit("listening"), 0);
      return this;
    }
    close() { if (this.fd >= 0) { pol.unwatch(this.fd); net._close(this.fd); this.fd = -1; this.emit("close"); } return this; }
  }
  net.createServer = (onConn) => { const s = new Server(); if (onConn) s.on("connection", onConn); return s; };
  net.connect = (port, host, cb) => {
    if (typeof host === "function") { cb = host; host = undefined; }
    host = host || "127.0.0.1";
    const s = new Socket(net._connect("tcp://" + host + ":" + port))._register();
    if (cb) s.once("connect", cb);
    g.setTimeout(() => s.emit("connect"), 0);   // connect resolved synchronously
    return s;
  };
  net.createConnection = net.connect;

  // --- dgram (UDP): connectionless, 'message' carries the sender rinfo ---
  class Dgram extends Emitter {
    constructor(type) { super(); this.type = type || "udp4"; this.fd = -1; this._rb = io.buf(64 << 10); }
    bind(port, addr, cb) {
      if (typeof addr === "function") { cb = addr; addr = undefined; }
      addr = addr || "0.0.0.0";
      this.fd = dgram._bind("udp://" + addr + ":" + port);
      if (cb) this.on("listening", cb);
      pol.watch(this.fd, pol.IN, (fd) => {
        for (;;) {
          if (this.fd < 0) return 0;       // closed by a prior 'message' handler
          this._rb.reset();
          const r = dgram._recv(fd, this._rb.idle());
          if (!r) break;                   // EAGAIN: drained
          this._rb.fed(r.n);
          this.emit("message", this._rb.data(), { address: r.address, port: r.port });
        }
        return this.fd < 0 ? 0 : pol.IN;
      });
      g.setTimeout(() => this.emit("listening"), 0);
      return this;
    }
    send(data, port, host) {
      const u = (typeof data === "string") ? utf8.Encode(data)
              : (data instanceof Buf) ? data.data() : data;
      dgram._send(this.fd, u, host || "127.0.0.1", port);
      return this;
    }
    close() { if (this.fd >= 0) { pol.unwatch(this.fd); net._close(this.fd); this.fd = -1; this.emit("close"); } return this; }
  }
  dgram.createSocket = (type, onMsg) => {
    if (typeof type === "object" && type) { onMsg = onMsg || type.onMessage; type = type.type; }
    const s = new Dgram(type);
    if (onMsg) s.on("message", onMsg);
    return s;
  };

  // --- Node timers over the single pol timer (a JS timer wheel) ---
  const TW = new Map();        // id -> { dueNs, periodNs, fn, args }
  let twId = 1, twOn = false;
  const MS = 1e6;              // ns per ms
  function twTick() {
    const now = pol.now();
    const due = [];
    for (const e of TW) if (e[1].dueNs <= now) due.push(e);
    for (const [id, t] of due) {
      if (t.periodNs > 0) t.dueNs = pol.now() + t.periodNs; else TW.delete(id);
      t.fn.apply(null, t.args);              // a throw propagates out of pol.run (Node: uncaught)
    }
    if (TW.size === 0) { twOn = false; return 3600001; }   // remove the pol timer
    let soon = Infinity;
    for (const t of TW.values()) if (t.dueNs < soon) soon = t.dueNs;
    const ms = Math.ceil((soon - pol.now()) / MS);
    return ms < 1 ? 1 : ms;
  }
  function twArm(ms) { if (!twOn) { twOn = true; pol.timer(twTick); } pol.sooner(ms < 1 ? 1 : ms | 0); }
  g.setTimeout = (fn, ms, ...args) => { ms = Math.max(0, +ms || 0); const id = twId++;
    TW.set(id, { dueNs: pol.now() + ms * MS, periodNs: 0, fn, args }); twArm(ms); return id; };
  g.setInterval = (fn, ms, ...args) => { ms = Math.max(1, +ms || 1); const id = twId++;
    TW.set(id, { dueNs: pol.now() + ms * MS, periodNs: ms * MS, fn, args }); twArm(ms); return id; };
  g.clearTimeout = (id) => { TW.delete(id); };
  g.clearInterval = g.clearTimeout;
  g.setImmediate = (fn, ...args) => g.setTimeout(fn, 0, ...args);
})(this);
)JS";

ok64 JABCNetInstall() {
  JABC_API_OBJECT(net);
  JABC_API_FN(net, "_listen", JABCNetListen);
  JABC_API_FN(net, "_connect", JABCNetConnect);
  JABC_API_FN(net, "_accept", JABCNetAccept);
  JABC_API_FN(net, "_recv", JABCNetRecv);
  JABC_API_FN(net, "_send", JABCNetSend);
  JABC_API_FN(net, "_shutwr", JABCNetShutwr);
  JABC_API_FN(net, "_close", JABCNetClose);
  JABC_API_OBJECT(dgram);
  JABC_API_FN(dgram, "_bind", JABCDgramBind);
  JABC_API_FN(dgram, "_recv", JABCDgramRecv);
  JABC_API_FN(dgram, "_send", JABCDgramSend);
  signal(SIGPIPE, SIG_IGN);  //  a write to a closed peer -> EPIPE, not a signal
  JABCExecute(JABC_NET_JS);
  return OK;
}
