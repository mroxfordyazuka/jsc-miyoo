/* Engine backend: JavaScriptCore (WebKit's engine, JSCOnly port). Large, but it compiles hot JavaScript to ARM
   machine code, which is what the 3D games need. Uses only JavaScriptCore's C API. */
#include "js.h"
#include <JavaScriptCore/JavaScript.h>
/* from JSContextRefPrivate.h (exported by the library, but its header is not installed) */
extern "C" {
typedef bool (*JSShouldTerminateCallback)(JSContextRef ctx, void *context);
void JSContextGroupSetExecutionTimeLimit(JSContextGroupRef group, double limit, JSShouldTerminateCallback callback, void *context);
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

static JSContextGroupRef group;
static JSGlobalContextRef ctx;
static JSObjectRef f_frame, f_pixels;

extern "C" const char *js_engine_name(void) { return "JavaScriptCore"; }

static std::string text_of(JSContextRef c, JSValueRef v) {
    if (!v) return "";
    JSValueRef ex = 0; JSStringRef s = JSValueToStringCopy(c, v, &ex);
    if (!s) return "";
    size_t cap = JSStringGetMaximumUTF8CStringSize(s);
    std::string out(cap, 0);
    size_t n = JSStringGetUTF8CString(s, &out[0], cap);
    JSStringRelease(s);
    out.resize(n ? n - 1 : 0);
    return out;
}
static JSValueRef str(JSContextRef c, const char *t) {
    JSStringRef s = JSStringCreateWithUTF8CString(t ? t : "");
    JSValueRef v = JSValueMakeString(c, s);
    JSStringRelease(s);
    return v;
}
static JSValueRef prop(JSContextRef c, JSObjectRef o, const char *name) {
    JSStringRef s = JSStringCreateWithUTF8CString(name); JSValueRef ex = 0;
    JSValueRef v = JSObjectGetProperty(c, o, s, &ex);
    JSStringRelease(s);
    return ex ? 0 : v;
}
static int int_of(JSContextRef c, JSValueRef v) { JSValueRef ex = 0; double d = JSValueToNumber(c, v, &ex); return ex || d != d ? 0 : (int)d; }
static void take_error(JSContextRef c, JSValueRef ex) {
    std::string msg, stack;
    if (ex && JSValueIsObject(c, ex)) {
        JSObjectRef o = (JSObjectRef)ex;
        JSValueRef m = prop(c, o, "message"), s = prop(c, o, "stack");
        if (m && JSValueIsString(c, m)) msg = text_of(c, m);
        if (s && JSValueIsString(c, s)) stack = text_of(c, s);
    }
    if (msg.empty()) msg = text_of(c, ex);
    /* code made with new Function has no file name here, so the stack gives no line for the game; the error's own
       "line" does, when it was thrown in the game itself (and not inside the engine, which has a sourceURL) */
    if (ex && JSValueIsObject(c, ex)) {
        JSObjectRef o = (JSObjectRef)ex;
        JSValueRef line = prop(c, o, "line"), url = prop(c, o, "sourceURL");
        std::string u = url && JSValueIsString(c, url) ? text_of(c, url) : "";
        JSValueRef name = prop(c, o, "name");
        bool syntax = name && JSValueIsString(c, name) && text_of(c, name) == "SyntaxError";     /* reported against the game's own text */
        if (line && JSValueIsNumber(c, line) && (u.empty() || syntax)) stack = "<input>:" + std::to_string(int_of(c, line) + 1) + "\n" + stack;
    }
    h_error(msg.c_str(), stack.c_str());
}
/* typed array -> its bytes */
static uint8_t *bytes_of(JSContextRef c, JSValueRef v, size_t *n) {
    if (!v || !JSValueIsObject(c, v)) return 0;
    JSObjectRef o = (JSObjectRef)v; JSValueRef ex = 0;
    if (JSValueGetTypedArrayType(c, v, &ex) == kJSTypedArrayTypeNone) return 0;
    uint8_t *base = (uint8_t *)JSObjectGetTypedArrayBytesPtr(c, o, &ex);
    if (!base || ex) return 0;
    *n = JSObjectGetTypedArrayByteLength(c, o, &ex);
    return base + JSObjectGetTypedArrayByteOffset(c, o, &ex);
}

#define FN(name) static JSValueRef name(JSContextRef c, JSObjectRef, JSObjectRef, size_t argc, const JSValueRef argv[], JSValueRef *)
FN(n_play) {
    size_t n = 0; if (argc < 2) return JSValueMakeUndefined(c);
    uint8_t *b = bytes_of(c, argv[1], &n);
    if (b && n >= 8 && !((uintptr_t)b & 7)) h_play(int_of(c, argv[0]), (const double *)b, n / 8);
    return JSValueMakeUndefined(c);
}
FN(n_bind) {
    size_t n[7]; uint8_t *p[7];
    if (argc < 8) return JSValueMakeUndefined(c);
    for (int i = 0; i < 7; i++) if (!(p[i] = bytes_of(c, argv[i], &n[i]))) return JSValueMakeUndefined(c);
    h_bind(p, n, int_of(c, argv[7]));
    return JSValueMakeUndefined(c);
}
FN(n_render) { if (argc > 0) h_render(int_of(c, argv[0])); return JSValueMakeUndefined(c); }
FN(n_now) { (void)argc; (void)argv; return JSValueMakeNumber(c, h_now()); }
FN(n_log) { for (size_t i = 0; i < argc; i++) h_log(text_of(c, argv[i]).c_str()); return JSValueMakeUndefined(c); }
FN(n_decode) {
    if (argc < 1) return JSValueMakeNull(c);
    char *src = h_decode(text_of(c, argv[0]).c_str());
    if (!src) return JSValueMakeNull(c);
    JSValueRef v = str(c, src); free(src); return v;
}
FN(n_launch) { if (argc >= 1) h_launch(text_of(c, argv[0]).c_str(), argc > 1 ? text_of(c, argv[1]).c_str() : ""); return JSValueMakeUndefined(c); }
FN(n_quit) { (void)argc; (void)argv; h_quit(); return JSValueMakeUndefined(c); }
FN(n_raw) { (void)argc; (void)argv; return JSValueMakeNumber(c, h_raw()); }
FN(n_remember) { if (argc >= 2) h_remember(int_of(c, argv[0]), int_of(c, argv[1])); return JSValueMakeUndefined(c); }
FN(n_update) { (void)argc; (void)argv; h_update(); return JSValueMakeUndefined(c); }
FN(n_serve) { return JSValueMakeBoolean(c, h_serve(argc > 0 ? int_of(c, argv[0]) : 0)); }   /* true / 1 = phone, 2 = dev panel too */
FN(n_addr) { (void)argc; (void)argv; return str(c, h_addr()); }
FN(n_inbox) { (void)argc; (void)argv; return JSValueMakeNumber(c, h_inbox()); }
FN(n_ui) {
    int v[5] = { 0, 0, 0, 0, 0 };
    if (argc > 0) v[0] = int_of(c, argv[0]);
    for (size_t i = 0; i < 4; i++) if (argc > 3 + i) v[1 + i] = int_of(c, argv[3 + i]);
    std::string s1 = argc > 1 ? text_of(c, argv[1]) : "", s2 = argc > 2 ? text_of(c, argv[2]) : "";
    return str(c, h_ui(v[0], s1.c_str(), s2.c_str(), v[1], v[2], v[3], v[4]));
}
FN(n_mine) { (void)argc; (void)argv; char *t = h_mine(); JSValueRef v = str(c, t); free(t); return v; }

static bool should_stop(JSContextRef, void *) { return h_timed_out(); }

extern "C" void js_stop(void) {
    if (!ctx) return;
    JSValueUnprotect(ctx, f_frame); JSValueUnprotect(ctx, f_pixels);
    f_frame = f_pixels = 0;
    JSGlobalContextRelease(ctx); ctx = 0;
    JSContextGroupRelease(group); group = 0;
}
static JSValueRef eval(JSContextRef c, const char *code, const char *url, JSValueRef *ex) {
    JSStringRef s = JSStringCreateWithUTF8CString(code), u = JSStringCreateWithUTF8CString(url);
    JSValueRef v = JSEvaluateScript(c, s, 0, u, 1, ex);
    JSStringRelease(s); JSStringRelease(u);
    return v;
}
extern "C" int js_start(const char *bundle, size_t, const char *source, int wide, const JsMenu *menu, int fixed_random) {
    static const struct { const char *name; JSObjectCallAsFunctionCallback f; } natives[] = {
        { "play", n_play }, { "bind", n_bind }, { "render", n_render }, { "now", n_now }, { "log", n_log }, { "decode", n_decode }, { "launch", n_launch }, { "quit", n_quit },
        { "raw", n_raw }, { "remember", n_remember }, { "update", n_update }, { "serve", n_serve }, { "addr", n_addr }, { "inbox", n_inbox }, { "mine", n_mine }, { "ui", n_ui } };
    group = JSContextGroupCreate();
    ctx = JSGlobalContextCreateInGroup(group, 0);
    JSContextGroupSetExecutionTimeLimit(group, 0.5, should_stop, 0);   /* asks should_stop every half second of running script */
    JSContextRef c = ctx; JSValueRef ex = 0;
    if (fixed_random)
        eval(c, "(()=>{let s=12345;Math.random=()=>{s=(Math.imul(s,1103515245)+12345)>>>0;return s/4294967296};"
                "let t=0;Date.now=()=>1700000000000+(t+=16);})()", "fixed.js", &ex);
    JSObjectRef host = JSObjectMake(c, 0, 0);
    { JSStringRef k = JSStringCreateWithUTF8CString("engine"); JSObjectSetProperty(c, host, k, str(c, js_engine_name()), kJSPropertyAttributeNone, 0); JSStringRelease(k); }
    for (size_t i = 0; i < sizeof natives / sizeof natives[0]; i++) {
        JSStringRef name = JSStringCreateWithUTF8CString(natives[i].name);
        JSObjectSetProperty(c, host, name, JSObjectMakeFunctionWithCallback(c, name, natives[i].f), kJSPropertyAttributeNone, 0);
        JSStringRelease(name);
    }
    ex = 0;
    JSValueRef boot = eval(c, bundle, "playload.js", &ex);
    JSValueRef start = 0, ctl = 0;
    if (!ex && boot && JSValueIsObject(c, boot)) { JSValueRef a[1] = { host }; start = JSObjectCallAsFunction(c, (JSObjectRef)boot, 0, 1, a, &ex); }
    if (!ex && start && JSValueIsObject(c, start)) {
        JSValueRef a[8] = { str(c, source), JSValueMakeBoolean(c, wide), str(c, menu->games_json), str(c, menu->arcade_json),
                            JSValueMakeNumber(c, menu->tab), JSValueMakeNumber(c, menu->sel), str(c, menu->error), str(c, menu->head) };
        ctl = JSObjectCallAsFunction(c, (JSObjectRef)start, 0, 8, a, &ex);
    }
    if (!ex && ctl && JSValueIsObject(c, ctl)) {
        JSValueRef fr = prop(c, (JSObjectRef)ctl, "frame"), px = prop(c, (JSObjectRef)ctl, "pixels");
        if (fr && px && JSValueIsObject(c, fr) && JSValueIsObject(c, px)) {
            f_frame = (JSObjectRef)fr; f_pixels = (JSObjectRef)px;
            JSValueProtect(c, f_frame); JSValueProtect(c, f_pixels);
            return 0;
        }
    }
    if (ex) take_error(c, ex); else h_error("The engine did not start", 0);
    f_frame = f_pixels = 0;
    JSGlobalContextRelease(ctx); ctx = 0; JSContextGroupRelease(group); group = 0;
    return -1;
}
extern "C" int js_frame(int mask) {
    JSValueRef ex = 0, a[1] = { JSValueMakeNumber(ctx, mask) };
    JSValueRef r = JSObjectCallAsFunction(ctx, f_frame, 0, 1, a, &ex);
    if (ex || !r) { take_error(ctx, ex); return -1; }
    int wait = int_of(ctx, r);
    return wait < 0 ? 0 : wait;
}
extern "C" int js_pixels(void) {
    JSValueRef ex = 0;
    JSValueRef r = JSObjectCallAsFunction(ctx, f_pixels, 0, 0, 0, &ex);
    if (ex || !r) { take_error(ctx, ex); return -1; }
    return 0;
}
