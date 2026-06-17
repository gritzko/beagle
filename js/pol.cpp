#include <math.h>
#include <poll.h>

#include "JABC.hpp"
extern "C" {
#include "abc/POL.h"
}

//  pol binds abc/POL — a thread-local poll(2) event loop.  JABC rule #4 holds:
//  C keeps NO per-fd JS closures.  The fd->handler table lives in the embedded
//  JS bundle (like Buf in buf.cpp); C holds just the two router refs below (the
//  bootstrap points) and routes every ready fd / timer tick through them.
//  Handlers do their own I/O via io.* — pol carries readiness, not bytes.
//  See POL.md for the API + contract.  v1: one timer (POL keys timers by C
//  callback pointer); many logical timers layer in JS.

//  The ONLY JS state C holds: the two routers, grabbed + protected at install.
static thread_local JSObjectRef JABC_POL_FD = NULL;     // pol._fd(fd, revents)->mask
static thread_local JSObjectRef JABC_POL_TIMER = NULL;  // pol._timer(ns)->next ms
//  A handler exception, stashed across the C loop and re-thrown by pol.run.
static thread_local JSValueRef JABC_POL_EXC = NULL;
//  YES while POLLoop is on the stack — guards re-entrant pol.init() (which
//  would POLFree the heap out from under the running loop).
static thread_local b8 JABC_POL_RUNNING = NO;

static int JABCInt(JSContextRef ctx, JSValueRef v, JSValueRef* e) {
  return (int)JSValueToNumber(ctx, v, e);
}

//  Set a numeric constant property (poll bits / time units).
static void JABCNum(JSObjectRef o, const char* name, double v) {
  JSStringRef k = JSStringCreateWithUTF8CString(name);
  JSObjectSetProperty(JABC_CONTEXT, o, k, JSValueMakeNumber(JABC_CONTEXT, v),
                      kJSPropertyAttributeReadOnly, NULL);
  JSStringRelease(k);
}

//  Call a protected router with up to two number args; return its Number
//  result, or -1 if the handler threw (caller stops the loop, stashes the exc).
static double JABCPolCall(JSObjectRef fn, double a0, double a1) {
  JSContextRef ctx = JABC_CONTEXT;
  JSValueRef argv[2] = {JSValueMakeNumber(ctx, a0), JSValueMakeNumber(ctx, a1)};
  JSValueRef exc = NULL;
  JSValueRef r = JSObjectCallAsFunction(ctx, fn, NULL, 2, argv, &exc);
  if (exc != NULL) {
    if (JABC_POL_EXC == NULL) {  //  keep the first throw; break the loop
      JABC_POL_EXC = exc;
      JSValueProtect(ctx, exc);
    }
    POLStop();
    return -1;
  }
  return JSValueToNumber(ctx, r, NULL);
}

//  fd trampoline — ONE C callback for ALL fds (POL keys fds by tofd, not by
//  pointer).  Route (fd, revents) to JS, return the next interest mask; 0 (or a
//  thrown handler) drops the fd.
static short JABCPolFd(int fd, poller* p) {
  double m = JABCPolCall(JABC_POL_FD, (double)fd, (double)p->revents);
  return m < 0 ? 0 : (short)m;
}

//  timer trampoline (the timercb passed to POLTrackTime) — POL keys timers by
//  this pointer, so one trampoline == one timer.  Return the next period in ms;
//  >= 1h (or a thrown handler) self-removes the timer.
static u32 JABCPolTimer(u64 ns) {
  double ms = JABCPolCall(JABC_POL_TIMER, (double)ns, 0);
  return (ms < 0 || !isfinite(ms)) ? 3600000u : (u32)ms;
}

//  ---- native leaves (the pol bundle wraps these as pol.watch/every/run/...) -

//  pol._watch(fd, events)
static JABC_FN(JABCPolWatch) {
  if (argc < 2) JABC_THROW("pol._watch(fd, events)");
  int fd = JABCInt(ctx, args[0], exception);
  short ev = (short)JSValueToNumber(ctx, args[1], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  poller p = {};
  p.callback = JABCPolFd;
  p.tofd = fd;
  p.events = ev;
  if (POLTrackEvents(fd, p) != OK) JABC_THROW("pol.watch failed");
  JABC_UNDEF;
}

//  pol._more(fd, events)
static JABC_FN(JABCPolMore) {
  if (argc < 2) JABC_THROW("pol._more(fd, events)");
  int fd = JABCInt(ctx, args[0], exception);
  short ev = (short)JSValueToNumber(ctx, args[1], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  if (POLAddEvents(fd, ev) != OK) JABC_THROW("pol.more: fd not tracked");
  JABC_UNDEF;
}

//  pol._unwatch(fd)
static JABC_FN(JABCPolUnwatch) {
  if (argc < 1) JABC_THROW("pol._unwatch(fd)");
  int fd = JABCInt(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  POLIgnoreEvents(fd);  //  POLNONE (untracked) is fine — idempotent drop
  JABC_UNDEF;
}

//  pol._every() — arm the single timer (period comes from the handler return)
static JABC_FN(JABCPolEvery) {
  (void)args;
  (void)argc;
  if (POLTrackTime(JABCPolTimer) != OK) JABC_THROW("pol.every failed");
  JABC_UNDEF;
}

//  pol.sooner(ms) — wake the timer earlier than scheduled
static JABC_FN(JABCPolSooner) {
  if (argc < 1) JABC_THROW("pol.sooner(ms)");
  int ms = JABCInt(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  POLAddTime(ms);  //  POLNONE (no timer) is fine — nothing to advance
  JABC_UNDEF;
}

//  pol._untimer()
static JABC_FN(JABCPolUntimer) {
  (void)args;
  (void)argc;
  POLIgnoreTime();
  JABC_UNDEF;
}

//  pol._run(ns) — drive the loop; ns < 0 or non-finite => POLNever (forever).
//  Re-throw a handler exception stashed during the run.
static JABC_FN(JABCPolRun) {
  double dns = argc ? JSValueToNumber(ctx, args[0], exception) : -1;
  if (*exception) return JSValueMakeUndefined(ctx);
  u64 ns = (dns < 0 || !isfinite(dns)) ? POLNever : (u64)dns;
  JABC_POL_RUNNING = YES;
  POLLoop(ns);
  JABC_POL_RUNNING = NO;
  if (JABC_POL_EXC != NULL) {
    *exception = JABC_POL_EXC;  //  surface the first handler throw to run()'s caller
    JSValueUnprotect(ctx, JABC_POL_EXC);
    JABC_POL_EXC = NULL;
    return JSValueMakeUndefined(ctx);
  }
  JABC_UNDEF;
}

static JABC_FN(JABCPolStop) {
  (void)args;
  (void)argc;
  POLStop();
  JABC_UNDEF;
}

static JABC_FN(JABCPolSleep) {
  if (argc < 1) JABC_THROW("pol.sleep(ns)");
  double dns = JSValueToNumber(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  if (dns > 0) POLSleep((u64)dns);
  JABC_UNDEF;
}

static JABC_FN(JABCPolAny) {
  (void)args;
  (void)argc;
  return JSValueMakeBoolean(ctx, POLAny());
}

static JABC_FN(JABCPolNow) {
  (void)args;
  (void)argc;
  return JSValueMakeNumber(ctx, (double)POLNow());
}

//  pol.init(maxfd) — (re)size the fd table.  Refused while a loop is live.
static JABC_FN(JABCPolInit) {
  int mx = argc ? JABCInt(ctx, args[0], exception) : 4096;
  if (*exception) return JSValueMakeUndefined(ctx);
  if (JABC_POL_RUNNING) JABC_THROW("pol.init: called from inside the loop");
  POLFree();
  if (POLInit(mx) != OK) JABC_THROW("pol.init failed");
  JABC_UNDEF;
}

//  The fd->handler table + the wrappers + the two routers live here (the only
//  per-fd state, all JS-owned).  C grabs pol._fd / pol._timer after this runs.
static const char* JABC_POL_JS = R"JS(
(function (g) {
  "use strict";
  const pol = g.pol;
  const table = new Map();     // fd -> handler(fd, revents) -> next mask
  let timer = null;            // the single timer handler(ns) -> next ms
  pol.default = null;          // catch-all for fds watched without a handler

  pol.watch = (fd, events, fn) => { if (fn) table.set(fd, fn); pol._watch(fd, events); return fd; };
  pol.more = (fd, events) => { pol._more(fd, events); };
  pol.unwatch = (fd) => { table.delete(fd); pol._unwatch(fd); };

  //  raw single-timer hook: fn(deadlineNs) -> next ms (>=1h removes).  every/
  //  after are sugar; the net timer-wheel drives pol.timer directly.
  pol.timer = (fn) => { timer = fn; pol._every(); };
  pol.every = (ms, fn) => pol.timer((ns) => { fn(ns); return ms; });
  pol.after = (ms, fn) => { pol.timer((ns) => { fn(ns); return 3600001; }); pol.sooner(ms); };
  pol.untimer = () => { timer = null; pol._untimer(); };

  pol.run = (ns) => pol._run(ns === undefined ? -1 : ns);
  pol.init = (maxfd) => { table.clear(); timer = null; pol.default = null; pol._init((maxfd | 0) || 4096); };

  //  routers — the ONLY entry points C calls back into (held as protected refs)
  pol._fd = (fd, revents) => {
    const h = table.get(fd) || pol.default;
    if (!h) { table.delete(fd); return 0; }    // unclaimed fd -> drop it
    const m = h(fd, revents) | 0;
    if (m === 0) table.delete(fd);             // handler released the fd
    return m;
  };
  pol._timer = (ns) => {
    if (!timer) return 3600001;                // no timer -> self-remove
    const ms = timer(ns) | 0;
    if (ms >= 3600000) timer = null;
    return ms;
  };
})(this);
)JS";

//  Look up a function property of `pol` and protect it as a C-held router ref.
static JSObjectRef JABCPolGrab(JSObjectRef pol, const char* name) {
  JSStringRef k = JSStringCreateWithUTF8CString(name);
  JSValueRef v = JSObjectGetProperty(JABC_CONTEXT, pol, k, NULL);
  JSStringRelease(k);
  JSObjectRef fn = JSValueToObject(JABC_CONTEXT, v, NULL);
  JSValueProtect(JABC_CONTEXT, fn);
  return fn;
}

ok64 JABCPolInstall() {
  POLInit(4096);  //  default fd table; pol.init() resizes
  JABC_API_OBJECT(pol);
  JABC_API_FN(pol, "_watch", JABCPolWatch);
  JABC_API_FN(pol, "_more", JABCPolMore);
  JABC_API_FN(pol, "_unwatch", JABCPolUnwatch);
  JABC_API_FN(pol, "_every", JABCPolEvery);
  JABC_API_FN(pol, "sooner", JABCPolSooner);
  JABC_API_FN(pol, "_untimer", JABCPolUntimer);
  JABC_API_FN(pol, "_run", JABCPolRun);
  JABC_API_FN(pol, "stop", JABCPolStop);
  JABC_API_FN(pol, "sleep", JABCPolSleep);
  JABC_API_FN(pol, "any", JABCPolAny);
  JABC_API_FN(pol, "now", JABCPolNow);
  JABC_API_FN(pol, "_init", JABCPolInit);

  //  poll(2) interest bits (platform values) + time units (ns)
  JABCNum(pol, "IN", POLLIN);
  JABCNum(pol, "OUT", POLLOUT);
  JABCNum(pol, "ERR", POLLERR);
  JABCNum(pol, "HUP", POLLHUP);
  JABCNum(pol, "PRI", POLLPRI);
  JABCNum(pol, "NVAL", POLLNVAL);
  JABCNum(pol, "SEC", (double)POLNanosPerSec);
  JABCNum(pol, "MS", (double)POLNanosPerMSec);
  JABCNum(pol, "NEVER", -1);  //  pol._run maps < 0 -> POLNever

  JABCExecute(JABC_POL_JS);
  JABC_POL_FD = JABCPolGrab(pol, "_fd");
  JABC_POL_TIMER = JABCPolGrab(pol, "_timer");
  return OK;
}

//  Called before the context is released (POLFree drops the heap; the routers
//  must be unprotected while the context is still alive).
ok64 JABCPolUninstall() {
  if (JABC_POL_FD) JSValueUnprotect(JABC_CONTEXT, JABC_POL_FD);
  if (JABC_POL_TIMER) JSValueUnprotect(JABC_CONTEXT, JABC_POL_TIMER);
  JABC_POL_FD = NULL;
  JABC_POL_TIMER = NULL;
  POLFree();
  return OK;
}
