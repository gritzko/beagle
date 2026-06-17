#ifndef ABC_JS_H
#define ABC_JS_H
#include <stdlib.h>

//  Include ABC C headers OUTSIDE the namespace first so system types
//  (struct timespec, etc.) land in the global namespace; ABC.hpp then
//  re-includes them (guarded) inside namespace abc.
extern "C" {
#include "abc/ABC.h"
}

#include "JavaScriptCore/JSBase.h"
#include "JavaScriptCore/JSContextRef.h"
#include "JavaScriptCore/JSObjectRef.h"
#include "JavaScriptCore/JSStringRef.h"
#include "JavaScriptCore/JSTypedArray.h"
#include "JavaScriptCore/JSValueRef.h"
#include "abc/ABC.hpp"

using namespace abc;

extern thread_local JSGlobalContextRef JABC_CONTEXT;
extern thread_local JSObjectRef JABC_GLOBAL_OBJECT;

//  A native binding function.  Its return value IS the JS result; on error
//  it sets *exception (via JABC_THROW) and returns undefined.
#define JABC_FN(fn)                                                       \
  JSValueRef fn(JSContextRef ctx, JSObjectRef function, JSObjectRef self, \
                size_t argc, const JSValueRef args[], JSValueRef* exception)

#define JABC_UNDEF return JSValueMakeUndefined(ctx)

//  Throw a JS Error with `msg` and return undefined.
#define JABC_THROW(msg)                                  \
  do {                                                   \
    JSStringRef m_ = JSStringCreateWithUTF8CString(msg); \
    JSValueRef v_ = JSValueMakeString(ctx, m_);          \
    *exception = JSObjectMakeError(ctx, 1, &v_, NULL);   \
    JSStringRelease(m_);                                 \
    return JSValueMakeUndefined(ctx);                    \
  } while (0)

//  Register a fresh API object `o` as a global of the same name.
#define JABC_API_OBJECT(o)                                           \
  JSObjectRef o = JSObjectMake(JABC_CONTEXT, NULL, NULL);            \
  {                                                                  \
    JSStringRef n_ = JSStringCreateWithUTF8CString(#o);              \
    JSObjectSetProperty(JABC_CONTEXT, JABC_GLOBAL_OBJECT, n_, o,     \
                        kJSPropertyAttributeNone, NULL);             \
    JSStringRelease(n_);                                             \
  }

//  Attach native function `f` as method `n` of object `o`.
#define JABC_API_FN(o, n, f)                                                  \
  {                                                                           \
    JSStringRef fn_ = JSStringCreateWithUTF8CString(n);                       \
    JSObjectSetProperty(JABC_CONTEXT, o, fn_,                                 \
                        JSObjectMakeFunctionWithCallback(JABC_CONTEXT, fn_, f),\
                        kJSPropertyAttributeNone, NULL);                      \
    JSStringRelease(fn_);                                                     \
  }

//  Make a JS string value from a C string (used for error text / small keys).
JSValueRef JSOfCString(const char* str);

//  Read a typed array's backing range into a binary slice.  Sets *exception
//  and returns NO on a non-typed-array / detached (NULL ptr) argument.
b8 JABCBytesOf(u8s out, JSContextRef ctx, JSValueRef arg, JSValueRef* exception);

void JABCExecute(const char* script);
void JABCReport(JSValueRef exception);

ok64 JABCutf8Install();
ok64 JABCioInstall();
ok64 JABCbufInstall();
ok64 JABCioUninstall();

#endif
