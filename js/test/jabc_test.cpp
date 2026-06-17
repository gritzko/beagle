//  Table-driven harness over the buf / utf8 / io boundary.  Each row is a JS
//  expression that must evaluate to `true`; a thrown exception or a non-true
//  result fails the row.  Native-fd rows (pipe, mmap) inject fds/paths as JS
//  globals.  Exits non-zero on any failure so CTest sees a real bug.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include "JABC.hpp"
extern "C" {
#include "abc/PRO.h"
}

thread_local JSGlobalContextRef JABC_CONTEXT;
thread_local JSObjectRef JABC_GLOBAL_OBJECT;
u8 _pro_depth = 0;
extern "C" _Thread_local u8* ABC_BASS[4] = {};

static int failures = 0;

void JABCReport(JSValueRef exception) {
  char page[PAGESIZE];
  JSStringRef es = JSValueToStringCopy(JABC_CONTEXT, exception, NULL);
  JSStringGetUTF8CString(es, page, PAGESIZE);
  JSStringRelease(es);
  fprintf(stderr, "    exception: %s\n", page);
}

void JABCExecute(const char* script) {
  JSStringRef code = JSStringCreateWithUTF8CString(script);
  JSValueRef exception = NULL;
  JSEvaluateScript(JABC_CONTEXT, code, NULL, NULL, 1, &exception);
  if (exception != NULL) JABCReport(exception);
  JSStringRelease(code);
}

static void setGlobalNum(const char* name, double v) {
  JSStringRef k = JSStringCreateWithUTF8CString(name);
  JSObjectSetProperty(JABC_CONTEXT, JABC_GLOBAL_OBJECT, k,
                      JSValueMakeNumber(JABC_CONTEXT, v),
                      kJSPropertyAttributeNone, NULL);
  JSStringRelease(k);
}

static void setGlobalStr(const char* name, const char* v) {
  JSStringRef k = JSStringCreateWithUTF8CString(name);
  JSObjectSetProperty(JABC_CONTEXT, JABC_GLOBAL_OBJECT, k, JSOfCString(v),
                      kJSPropertyAttributeNone, NULL);
  JSStringRelease(k);
}

//  Evaluate `js`; pass iff no exception and the result is boolean true.
static void check(const char* name, const char* js) {
  JSStringRef code = JSStringCreateWithUTF8CString(js);
  JSValueRef exception = NULL;
  JSValueRef r = JSEvaluateScript(JABC_CONTEXT, code, NULL, NULL, 1, &exception);
  JSStringRelease(code);
  if (exception != NULL) {
    fprintf(stderr, "FAIL %s\n", name);
    JABCReport(exception);
    failures++;
    return;
  }
  if (!JSValueToBoolean(JABC_CONTEXT, r)) {
    fprintf(stderr, "FAIL %s (not true)\n", name);
    failures++;
    return;
  }
  fprintf(stderr, "ok   %s\n", name);
}

struct Row {
  const char* name;
  const char* js;
};

//  In-memory rows: utf8 + Buf, no fd.
static const Row ROWS[] = {
    {"encode_len", "utf8.Encode('hello').length===5"},
    {"encode_multibyte", "utf8.Encode('é').length===2"},
    {"encode_astral", "utf8.Encode('\U0001F600').length===4"},
    {"decode_roundtrip",
     "utf8.Decode(utf8.Encode('héllo ☃'))==='héllo ☃'"},
    {"decode_overlong",
     "(()=>{try{utf8.Decode(new Uint8Array([0xC0,0x80]));return false}"
     "catch(e){return true}})()"},
    {"decode_lone_surrogate",
     "(()=>{try{utf8.Decode(new Uint8Array([0xED,0xA0,0x80]));return false}"
     "catch(e){return true}})()"},
    {"encodeInto_partial_ascii",
     "(()=>{let b=io.buf(2);return b.feedStr('abc')===2&&b.size===2})()"},
    {"encodeInto_no_half_multibyte",
     "(()=>{let b=io.buf(1);return b.feedStr('é')===0&&b.size===0})()"},
    {"buf_feed1_feedStr",
     "(()=>{let b=io.buf(8);b.feed1(65);b.feedStr('BC');"
     "return b.size===3&&utf8.Decode(b.data())==='ABC'})()"},
    {"buf_feed_buf",
     "(()=>{let a=io.buf(8);a.feedStr('xy');let b=io.buf(8);b.feed(a);"
     "return utf8.Decode(b.data())==='xy'})()"},
    {"buf_take",
     "(()=>{let b=io.buf(8);b.feedStr('abcd');let v=b.take(2);"
     "return v.length===2&&v[0]===97&&b.size===2&&utf8.Decode(b.data())==='cd'})()"},
    {"buf_feed_noroom",
     "(()=>{let b=io.buf(2);try{b.feed(utf8.Encode('abc'));return false}"
     "catch(e){return true}})()"},
    {"buf_shift",
     "(()=>{let b=io.buf(8);b.feedStr('abcd');b.skip(2);b.shift();"
     "return b._data===0&&b.size===2&&utf8.Decode(b.data())==='cd'})()"},
    {"buf_pop",
     "(()=>{let b=io.buf(8);b.feedStr('abc');return b.pop()===99&&b.size===2})()"},
    {"buf_shed",
     "(()=>{let b=io.buf(8);b.feedStr('abcd');b.shed(2);"
     "return b.size===2&&utf8.Decode(b.data())==='ab'})()"},
    {"buf_splice",
     "(()=>{let b=io.buf(16);b.feedStr('hello');b.splice(1,3,utf8.Encode('XY'));"
     "return utf8.Decode(b.data())==='hXYo'})()"},
    {"buf_grow",
     "(()=>{let b=io.buf(2);b.feedStr('ab');b.grow(8);b.feedStr('cd');"
     "return b.cap>=8&&utf8.Decode(b.data())==='abcd'})()"},
    {"ram_roundtrip",
     "(()=>{let b=io.ram(1<<20);b.feedStr('ram');"
     "return b.size===3&&utf8.Decode(b.data())==='ram'})()"},
};

int main() {
  if (u8bMap(ABC_BASS, ABC_BASS_BYTES) != OK) {
    fprintf(stderr, "ABC_BASS u8bMap failed\n");
    return 1;
  }
  JABC_CONTEXT = JSGlobalContextCreate(NULL);
  JABC_GLOBAL_OBJECT = JSContextGetGlobalObject(JABC_CONTEXT);
  JABCutf8Install();
  JABCioInstall();
  JABCbufInstall();

  for (size_t i = 0; i < sizeof(ROWS) / sizeof(ROWS[0]); i++)
    check(ROWS[i].name, ROWS[i].js);

  //  fd round-trip over a pipe: write a Buf's DATA, read it back into a Buf.
  int fds[2];
  if (pipe(fds) == 0) {
    setGlobalNum("WFD", fds[1]);
    setGlobalNum("RFD", fds[0]);
    check("pipe_write_read",
          "(()=>{let w=io.buf(16);w.feedStr('ping');io.writeAll(WFD,w);"
          "let r=io.buf(16);io.read(RFD,r);"
          "return r.size===4&&utf8.Decode(r.data())==='ping'})()");
    close(fds[0]);
    close(fds[1]);
  }

  //  mmap round-trip: write a file via fd, map it RO, read the bytes back.
  char path[] = "/tmp/jabc_mmap_XXXXXX";
  int tmp = mkstemp(path);
  if (tmp >= 0) {
    close(tmp);
    setGlobalStr("MMAP_PATH", path);
    check("mmap_roundtrip",
          "(()=>{let fd=io.open(MMAP_PATH,'c');let w=io.buf(16);"
          "w.feedStr('mapped!');io.writeAll(fd,w);io.close(fd);"
          "let m=io.mmap(MMAP_PATH,'r');"
          "return m.size===7&&utf8.Decode(m.data())==='mapped!'})()");
    unlink(path);
  }

  //  Release the context FIRST: GC runs the no-copy deallocators
  //  (FILEUnMap/munmap), which touch the FILE subsystem — so tear that down
  //  (FILECloseAll frees FILE_RW) only afterwards.
  JSGlobalContextRelease(JABC_CONTEXT);
  JABCioUninstall();
  u8bUnMap(ABC_BASS);

  fprintf(stderr, "\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
          failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
