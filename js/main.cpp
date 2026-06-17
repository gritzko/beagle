#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "JABC.hpp"
extern "C" {
#include "abc/PRO.h"
}

thread_local JSGlobalContextRef JABC_CONTEXT;
thread_local JSObjectRef JABC_GLOBAL_OBJECT;

//  PRO.h globals (one definition for the whole binary).
u8 _pro_depth = 0;
extern "C" _Thread_local u8* ABC_BASS[4] = {};

static JSValueRef JSPropertyMessage = NULL;
static JSValueRef JSPropertyStack = NULL;

static void JSInit() {
  JABC_CONTEXT = JSGlobalContextCreate(NULL);
  JABC_GLOBAL_OBJECT = JSContextGetGlobalObject(JABC_CONTEXT);

  JSStringRef m = JSStringCreateWithUTF8CString("message");
  JSPropertyMessage = JSValueMakeString(JABC_CONTEXT, m);
  JSStringRelease(m);
  JSValueProtect(JABC_CONTEXT, JSPropertyMessage);

  JSStringRef s = JSStringCreateWithUTF8CString("stack");
  JSPropertyStack = JSValueMakeString(JABC_CONTEXT, s);
  JSStringRelease(s);
  JSValueProtect(JABC_CONTEXT, JSPropertyStack);
}

static void JSClose() {
  JSValueUnprotect(JABC_CONTEXT, JSPropertyStack);
  JSValueUnprotect(JABC_CONTEXT, JSPropertyMessage);
  JSGlobalContextRelease(JABC_CONTEXT);
  JABC_CONTEXT = NULL;
  JABC_GLOBAL_OBJECT = NULL;
  JSPropertyMessage = NULL;
  JSPropertyStack = NULL;
}

//  Report a JS exception (message + stack if present) to stderr.
void JABCReport(JSValueRef exception) {
  char page[PAGESIZE];
  if (JSValueIsObject(JABC_CONTEXT, exception) &&
      JSObjectHasPropertyForKey(JABC_CONTEXT, (JSObjectRef)exception,
                                JSPropertyStack, NULL)) {
    JSValueRef ref = JSObjectGetPropertyForKey(
        JABC_CONTEXT, (JSObjectRef)exception, JSPropertyStack, NULL);
    JSStringRef rs = JSValueToStringCopy(JABC_CONTEXT, ref, NULL);
    JSStringGetUTF8CString(rs, page, PAGESIZE);
    JSStringRelease(rs);
    fprintf(stderr, "JS exception: %s\n", page);
    return;
  }
  JSStringRef es = JSValueToStringCopy(JABC_CONTEXT, exception, NULL);
  JSStringGetUTF8CString(es, page, PAGESIZE);
  JSStringRelease(es);
  fprintf(stderr, "JS exception: %s\n", page);
}

void JABCExecute(const char* script) {
  JSStringRef code = JSStringCreateWithUTF8CString(script);
  JSValueRef exception = NULL;
  JSEvaluateScript(JABC_CONTEXT, code, NULL, NULL, 1, &exception);
  if (exception != NULL) JABCReport(exception);
  JSStringRelease(code);
}

//  Like JABCExecute but reports whether an uncaught exception occurred, so a
//  failing `--eval`/script run propagates to the process exit code (CTest).
static b8 JABCRun(const char* script) {
  JSStringRef code = JSStringCreateWithUTF8CString(script);
  JSValueRef exception = NULL;
  JSEvaluateScript(JABC_CONTEXT, code, NULL, NULL, 1, &exception);
  JSStringRelease(code);
  if (exception != NULL) {
    JABCReport(exception);
    return NO;
  }
  return YES;
}

static void JABCInstallModules() {
  JABCutf8Install();
  JABCioInstall();
  JABCbufInstall();   //  Buf class + constructors layer over utf8/io leaves
  JABCContInstall();  //  abc.* containers (HEAP/HASH) layer over io mmap leaves
  JABCUriInstall();   //  URI class over abc/URI
  JABCCodecInstall(); //  hex + sha1/sha256 + ron
  JABCAnsiInstall();  //  ansi colour helper (pure JS)
  JABCPolInstall();   //  poll() event loop over abc/POL (handlers do io.* I/O)
  JABCNetInstall();   //  net/dgram + Node timers over pol (sockets do io.* I/O)
  JABCRequireInstall(); //  sync CommonJS require() over io.mmap/utf8 (last)
}

#define VERSION_BOILERPLATE "jabc v0.1.0\n"

int main(int argc, char** argv) {
  if (u8bMap(ABC_BASS, ABC_BASS_BYTES) != OK) {
    fprintf(stderr, "ABC_BASS u8bMap failed\n");
    return 1;
  }

  char* eval_code = NULL;
  char* script_file = NULL;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--version") == 0) {
      fprintf(stderr, VERSION_BOILERPLATE);
      return 0;
    } else if (strcmp(argv[i], "--eval") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --eval requires a code argument\n");
        return 1;
      }
      eval_code = argv[++i];
    } else {
      script_file = argv[i];
      break;
    }
  }

  JSInit();
  JABCInstallModules();

  int rc = 0;
  if (eval_code != NULL && !JABCRun(eval_code)) rc = 1;

  if (script_file != NULL) {
    FILE* f = fopen(script_file, "rb");
    if (!f) {
      fprintf(stderr, "Error: cannot open %s\n", script_file);
      return 1;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0) {
      fprintf(stderr, "Error: cannot size %s\n", script_file);
      fclose(f);
      return 1;
    }
    rewind(f);
    char* script = (char*)malloc((size_t)len + 1);
    if (!script) {
      fprintf(stderr, "Error: out of memory reading %s\n", script_file);
      fclose(f);
      return 1;
    }
    size_t got = fread(script, 1, (size_t)len, f);
    fclose(f);
    script[got] = '\0';
    if (!JABCRun(script)) rc = 1;
    free(script);
  }

  //  Node-like: once the top-level script returns, drive the event loop until
  //  no fds/timers remain.  pol.run on an already-drained queue is an instant
  //  no-op, so scripts that never touch net/timers are unaffected.
  if (rc == 0 && !JABCRun("pol.run(pol.NEVER)")) rc = 1;

  //  Drop the protected pol router refs + free the poll heap while the context
  //  is still alive (they are JS values).
  JABCPolUninstall();
  //  Release the context first so GC runs the no-copy deallocators
  //  (FILEUnMap/munmap) while the FILE subsystem is still alive; then tear it
  //  down (FILECloseAll frees FILE_RW).
  JSClose();
  JABCioUninstall();
  u8bUnMap(ABC_BASS);
  return rc;
}
