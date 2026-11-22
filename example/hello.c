/* The smallest useful embedding of JavaScriptCore: run a script, give it one function of ours, call one of its
   functions every "frame". Plain C against the public C API (include/JavaScriptCore).
   Build: ./build.sh     Run on the Miyoo: ./hello            (prints to the terminal / log) */
#include <JavaScriptCore/JavaScript.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000.0 + t.tv_nsec / 1e6; }

/* print(text) for the script */
static JSValueRef js_print(JSContextRef ctx, JSObjectRef fn, JSObjectRef self, size_t argc, const JSValueRef argv[], JSValueRef *err) {
    (void)fn; (void)self;
    if (argc > 0) {
        JSStringRef s = JSValueToStringCopy(ctx, argv[0], err);
        if (s) { char buf[512]; JSStringGetUTF8CString(s, buf, sizeof buf); puts(buf); JSStringRelease(s); }
    }
    return JSValueMakeUndefined(ctx);
}
static void report(JSContextRef ctx, JSValueRef err) {
    JSStringRef s = JSValueToStringCopy(ctx, err, 0); char buf[512] = "error";
    if (s) { JSStringGetUTF8CString(s, buf, sizeof buf); JSStringRelease(s); }
    fprintf(stderr, "script error: %s\n", buf);
}

static const char SCRIPT[] =
    "let total = 0;\n"
    "function frame(n) {            // some arithmetic, so the JIT has something to compile\n"
    "  let s = 0;\n"
    "  for (let i = 0; i < 200000; i++) s = (s + i * n) % 1000003;\n"
    "  total += s;\n"
    "  return s;\n"
    "}\n"
    "print('script loaded');\n";

int main(void) {
    JSGlobalContextRef ctx = JSGlobalContextCreate(0);
    JSObjectRef global = JSContextGetGlobalObject(ctx);
    JSValueRef err = 0;

    JSStringRef name = JSStringCreateWithUTF8CString("print");
    JSObjectSetProperty(ctx, global, name, JSObjectMakeFunctionWithCallback(ctx, name, js_print), kJSPropertyAttributeNone, 0);
    JSStringRelease(name);

    JSStringRef src = JSStringCreateWithUTF8CString(SCRIPT);
    JSEvaluateScript(ctx, src, 0, 0, 1, &err);
    JSStringRelease(src);
    if (err) { report(ctx, err); return 1; }

    name = JSStringCreateWithUTF8CString("frame");
    JSObjectRef frame = JSValueToObject(ctx, JSObjectGetProperty(ctx, global, name, 0), 0);
    JSStringRelease(name);

    /* the first calls run in the interpreter; watch the time fall as the JIT takes over */
    for (int i = 1; i <= 30; i++) {
        JSValueRef arg = JSValueMakeNumber(ctx, i);
        double a = now_ms();
        JSValueRef r = JSObjectCallAsFunction(ctx, frame, 0, 1, &arg, &err);
        double took = now_ms() - a;
        if (err) { report(ctx, err); return 1; }
        if (i <= 5 || i % 5 == 0) printf("call %2d: %.2f ms (result %.0f)\n", i, took, JSValueToNumber(ctx, r, 0));
    }
    JSGlobalContextRelease(ctx);
    return 0;
}
