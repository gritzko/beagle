#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "JABC.hpp"
#include "abc/FILE.h"
#include "abc/PATH.h"

#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif

//  fds are plain numbers; buffers are JS-owned typed arrays.  These are the
//  leaf syscalls — one native call == one tight C operation over a fd and/or
//  a typed array's backing memory.  The cursor logic lives in the JS `Buf`
//  class (buf.js); nothing here holds memory or a JS reference.

static int JABCInt(JSContextRef ctx, JSValueRef v, JSValueRef* exception) {
  return (int)JSValueToNumber(ctx, v, exception);
}

//  Copy a JS-string path argument into a NUL-terminated path buffer.
static ok64 JABCPath(path8b path, JSContextRef ctx, JSValueRef arg,
                     JSValueRef* exception) {
  if (!JSValueIsString(ctx, arg)) return BADARG;
  JSStringRef s = JSValueToStringCopy(ctx, arg, exception);
  if (*exception || s == NULL) return BADARG;
  u8 page[PAGESIZE];
  size_t len = JSStringGetUTF8CString(s, (char*)page, PAGESIZE);
  JSStringRelease(s);
  if (len < 1 || len >= PAGESIZE) return NOROOM;  //  len counts the NUL
  u8cs src = {page, page + (len - 1)};
  ok64 o = u8bFeed(path, src);
  if (o != OK) return o;
  PATHu8bTerm(path);
  return OK;
}

//  io.open(path, "r"|"rw"|"c") -> fd
static JABC_FN(JABCioOpen) {
  if (argc < 1) JABC_THROW("io.open(path, mode) -> fd");
  a_pad(u8, path, FILE_PATH_MAX_LEN);
  if (JABCPath(path, ctx, args[0], exception) != OK) {
    if (*exception) return JSValueMakeUndefined(ctx);
    JABC_THROW("io.open(): bad path");
  }
  char mode[8] = "r";
  if (argc > 1 && JSValueIsString(ctx, args[1])) {
    JSStringRef s = JSValueToStringCopy(ctx, args[1], NULL);
    JSStringGetUTF8CString(s, mode, sizeof(mode));
    JSStringRelease(s);
  }
  int flags = O_RDONLY;
  if (strcmp(mode, "rw") == 0) flags = O_RDWR;
  else if (strcmp(mode, "c") == 0) flags = O_RDWR | O_CREAT | O_TRUNC;
  int fd = -1;
  ok64 o = FILEOpen(&fd, $path(path), flags);
  if (o != OK || fd < 0) JABC_THROW(strerror(errno));
  return JSValueMakeNumber(ctx, (double)fd);
}

//  io.close(fd)
static JABC_FN(JABCioClose) {
  if (argc < 1) JABC_THROW("io.close(fd)");
  int fd = JABCInt(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  FILEClose(&fd);
  JABC_UNDEF;
}

//  io.sync(fd)
static JABC_FN(JABCioSync) {
  if (argc < 1) JABC_THROW("io.sync(fd)");
  int fd = JABCInt(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  if (FILESync(&fd) != OK) JABC_THROW(strerror(errno));
  JABC_UNDEF;
}

//  io.size(fd) -> bytes
static JABC_FN(JABCioSize) {
  if (argc < 1) JABC_THROW("io.size(fd)");
  int fd = JABCInt(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  size_t sz = 0;
  if (FILESize(&sz, &fd) != OK) JABC_THROW(strerror(errno));
  return JSValueMakeNumber(ctx, (double)sz);
}

//  io.resize(fd, n)
static JABC_FN(JABCioResize) {
  if (argc < 2) JABC_THROW("io.resize(fd, n)");
  int fd = JABCInt(ctx, args[0], exception);
  double n = JSValueToNumber(ctx, args[1], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  if (n < 0) JABC_THROW("io.resize(): negative size");
  if (FILEResize(&fd, (size_t)n) != OK) JABC_THROW(strerror(errno));
  JABC_UNDEF;
}

//  io.lock(fd, exclusive?) / io.unlock(fd)
static JABC_FN(JABCioLock) {
  if (argc < 1) JABC_THROW("io.lock(fd, exclusive)");
  int fd = JABCInt(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  b8 excl = argc > 1 ? JSValueToBoolean(ctx, args[1]) : YES;
  if (FILELock(&fd, excl) != OK) JABC_THROW(strerror(errno));
  JABC_UNDEF;
}
static JABC_FN(JABCioUnlock) {
  if (argc < 1) JABC_THROW("io.unlock(fd)");
  int fd = JABCInt(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  if (FILEUnlock(&fd) != OK) JABC_THROW(strerror(errno));
  JABC_UNDEF;
}

//  io.stat(path) -> {size, mode, kind}
static JABC_FN(JABCioStat) {
  if (argc < 1) JABC_THROW("io.stat(path)");
  a_pad(u8, path, FILE_PATH_MAX_LEN);
  if (JABCPath(path, ctx, args[0], exception) != OK) {
    if (*exception) return JSValueMakeUndefined(ctx);
    JABC_THROW("io.stat(): bad path");
  }
  filestat fs = {};
  ok64 o = FILEStat(&fs, $path(path));
  if (o != OK) JABC_THROW(strerror(errno));
  JSObjectRef obj = JSObjectMake(ctx, NULL, NULL);
  JSStringRef k;
  k = JSStringCreateWithUTF8CString("size");
  JSObjectSetProperty(ctx, obj, k, JSValueMakeNumber(ctx, (double)fs.size),
                      kJSPropertyAttributeNone, NULL);
  JSStringRelease(k);
  k = JSStringCreateWithUTF8CString("mode");
  JSObjectSetProperty(ctx, obj, k, JSValueMakeNumber(ctx, (double)fs.mode),
                      kJSPropertyAttributeNone, NULL);
  JSStringRelease(k);
  const char* kind = "other";
  if (fs.kind == FILE_KIND_REG) kind = "reg";
  else if (fs.kind == FILE_KIND_DIR) kind = "dir";
  else if (fs.kind == FILE_KIND_LNK) kind = "lnk";
  k = JSStringCreateWithUTF8CString("kind");
  JSObjectSetProperty(ctx, obj, k, JSOfCString(kind), kJSPropertyAttributeNone,
                      NULL);
  JSStringRelease(k);
  return obj;
}

//  io._read(fd, u8) -> n  (0 = EOF).  Single read into the typed array.
static JABC_FN(JABCioRead) {
  if (argc < 2) JABC_THROW("io._read(fd, Uint8Array)");
  int fd = JABCInt(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  u8s ta = {};
  if (!JABCBytesOf(ta, ctx, args[1], exception)) return JSValueMakeUndefined(ctx);
  ssize_t n;
  do { n = read(fd, ta[0], $len(ta)); } while (n < 0 && errno == EINTR);
  if (n < 0) JABC_THROW(strerror(errno));
  return JSValueMakeNumber(ctx, (double)n);
}

//  io._write(fd, u8) -> n.  Single write of the typed array's bytes.
static JABC_FN(JABCioWrite) {
  if (argc < 2) JABC_THROW("io._write(fd, Uint8Array)");
  int fd = JABCInt(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  u8s ta = {};
  if (!JABCBytesOf(ta, ctx, args[1], exception)) return JSValueMakeUndefined(ctx);
  ssize_t n;
  do { n = write(fd, ta[0], $len(ta)); } while (n < 0 && errno == EINTR);
  if (n < 0) JABC_THROW(strerror(errno));
  return JSValueMakeNumber(ctx, (double)n);
}

//  Mapped-file deallocator: JS GC drives the munmap (buf is the FILE_WANT_BUFS
//  slot handed to FILEUnMap).
static void JABCMapFree(void* bytes, void* ctx) {
  (void)bytes;
  FILEUnMap((u8bp)ctx);
}

//  io._mmap(path, "r"|"rw"|"c", size) -> Uint8Array  (munmap on GC)
static JABC_FN(JABCioMmap) {
  if (argc < 1) JABC_THROW("io._mmap(path, mode, size)");
  a_pad(u8, path, FILE_PATH_MAX_LEN);
  if (JABCPath(path, ctx, args[0], exception) != OK) {
    if (*exception) return JSValueMakeUndefined(ctx);
    JABC_THROW("io._mmap(): bad path");
  }
  char mode[8] = "r";
  if (argc > 1 && JSValueIsString(ctx, args[1])) {
    JSStringRef s = JSValueToStringCopy(ctx, args[1], NULL);
    JSStringGetUTF8CString(s, mode, sizeof(mode));
    JSStringRelease(s);
  }
  u8bp buf = NULL;
  ok64 o;
  if (strcmp(mode, "c") == 0) {
    double sz = argc > 2 ? JSValueToNumber(ctx, args[2], NULL) : 0;
    if (sz < 0) JABC_THROW("io._mmap(): negative size");
    o = FILEMapCreate(&buf, $path(path), (size_t)sz);
  } else if (strcmp(mode, "rw") == 0) {
    o = FILEMapRW(&buf, $path(path));
  } else {
    o = FILEMapRO(&buf, $path(path));
  }
  if (o != OK || buf == NULL) JABC_THROW(strerror(errno));
  //  Expose the WHOLE mapping (DATA + IDLE): a created file has DATA empty and
  //  everything in IDLE, a mapped existing file has it all in DATA — either
  //  way the container owns the full region.
  size_t mlen = u8bDataLen(buf) + u8bIdleLen(buf);
  JSValueRef ta = JSObjectMakeTypedArrayWithBytesNoCopy(
      ctx, kJSTypedArrayTypeUint8Array, u8bData(buf)[0], mlen,
      JABCMapFree, (void*)buf, exception);
  if (*exception || ta == NULL) {
    FILEUnMap(buf);
    return JSValueMakeUndefined(ctx);
  }
  return ta;
}

//  Anonymous-mapping deallocator: munmap the whole region, free the length box.
static void JABCRamFree(void* bytes, void* ctx) {
  size_t* len = (size_t*)ctx;
  munmap(bytes, *len);
  free(len);
}

//  io._ram(n) -> Uint8Array  (anonymous mmap, faults in lazily, munmap on GC)
static JABC_FN(JABCioRam) {
  if (argc < 1) JABC_THROW("io._ram(n)");
  double dn = JSValueToNumber(ctx, args[0], exception);
  if (*exception) return JSValueMakeUndefined(ctx);
  if (dn <= 0) JABC_THROW("io._ram(): size must be > 0");
  size_t n = (size_t)dn;
  void* map = mmap(NULL, n, PROT_READ | PROT_WRITE,
                   MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE, -1, 0);
  if (map == MAP_FAILED) JABC_THROW(strerror(errno));
  size_t* len = (size_t*)malloc(sizeof(size_t));
  *len = n;
  JSValueRef ta = JSObjectMakeTypedArrayWithBytesNoCopy(
      ctx, kJSTypedArrayTypeUint8Array, map, n, JABCRamFree, len, exception);
  if (*exception || ta == NULL) {
    munmap(map, n);
    free(len);
    return JSValueMakeUndefined(ctx);
  }
  return ta;
}

//  io._msync(u8) -> flush the typed array's pages (works on the raw range,
//  no buffer-descriptor lookup needed).
static JABC_FN(JABCioMsync) {
  if (argc < 1) JABC_THROW("io._msync(Uint8Array)");
  u8s ta = {};
  if (!JABCBytesOf(ta, ctx, args[0], exception)) return JSValueMakeUndefined(ctx);
  if ($len(ta) && msync(ta[0], $len(ta), MS_SYNC) != 0)
    JABC_THROW(strerror(errno));
  JABC_UNDEF;
}

//  io.log(...args) -> write strings / typed arrays to stderr + newline.
static JABC_FN(JABCioLog) {
  for (size_t i = 0; i < argc; i++) {
    if (JSValueIsString(ctx, args[i])) {
      JSStringRef s = JSValueToStringCopy(ctx, args[i], exception);
      if (*exception) return JSValueMakeUndefined(ctx);
      size_t max = JSStringGetMaximumUTF8CStringSize(s);
      char* b = (char*)malloc(max);
      size_t got = JSStringGetUTF8CString(s, b, max);
      u8s span = {(u8*)b, (u8*)b + (got ? got - 1 : 0)};
      while ($len(span) > 0) {
        ssize_t w = write(STDERR_FILENO, span[0], $len(span));
        if (w <= 0) break;
        span[0] += w;
      }
      free(b);
      JSStringRelease(s);
    } else if (JSValueGetTypedArrayType(ctx, args[i], NULL) !=
               kJSTypedArrayTypeNone) {
      u8s ta = {};
      if (!JABCBytesOf(ta, ctx, args[i], exception))
        return JSValueMakeUndefined(ctx);
      ssize_t w = write(STDERR_FILENO, ta[0], $len(ta));
      (void)w;
    }
  }
  ssize_t w = write(STDERR_FILENO, "\n", 1);
  (void)w;
  JABC_UNDEF;
}

ok64 JABCioInstall() {
  FILEInit();
  JABC_API_OBJECT(io);
  JABC_API_FN(io, "open", JABCioOpen);
  JABC_API_FN(io, "close", JABCioClose);
  JABC_API_FN(io, "sync", JABCioSync);
  JABC_API_FN(io, "size", JABCioSize);
  JABC_API_FN(io, "resize", JABCioResize);
  JABC_API_FN(io, "lock", JABCioLock);
  JABC_API_FN(io, "unlock", JABCioUnlock);
  JABC_API_FN(io, "stat", JABCioStat);
  JABC_API_FN(io, "_read", JABCioRead);
  JABC_API_FN(io, "_write", JABCioWrite);
  JABC_API_FN(io, "_mmap", JABCioMmap);
  JABC_API_FN(io, "_ram", JABCioRam);
  JABC_API_FN(io, "_msync", JABCioMsync);
  JABC_API_FN(io, "log", JABCioLog);
  return OK;
}

ok64 JABCioUninstall() {
  FILECloseAll();
  return OK;
}
