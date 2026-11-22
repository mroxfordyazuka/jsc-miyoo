# JavaScriptCore (with JIT) for the Miyoo Mini / Mini Plus

```text
Safari's JavaScript engine, built as static libraries for the Miyoo's CPU (armv7 Cortex-A7, hard float, glibc,
miyoomini toolchain GCC 8.3). It is the engine inside Playload's playload-jit, which has run on a Miyoo Mini
Plus under Onion. Link it into any C or C++ program to run JavaScript there.

What is here
  lib/libJavaScriptCore.a, libWTF.a          the engine (WebKit tag webkitgtk-2.38.6, PORT=JSCOnly)
  lib/libicui18n.a, libicuuc.a, libicudata.a ICU 69.1, which the engine needs (Intl, Unicode tables)
  include/JavaScriptCore/                    the public C API (JavaScript.h pulls in the rest)
  example/hello.c, build.sh                  a minimal embedding in plain C; it builds, it has not been run on a Miyoo
  example/playload_js_jsc.cpp                Playload's real embedding: natives, typed arrays, error lines, a
                                             watchdog for scripts that hang (it needs Playload's js.h to compile;
                                             it is here to read)
  REBUILDING.txt, jsc-toolchain.cmake        how the libraries were built
  licenses/

How it was configured
  Interpreter + baseline JIT + DFG JIT. Off: the FTL tier, WebAssembly, the remote inspector, bmalloc
  (it uses the system malloc). The library code is Thumb-2, which the ARMv7 JIT requires; your own code can be
  -marm or -mthumb.

Using it
  - Compile with -Iinclude, link with g++ in the order build.sh shows. A program that only prints "hello" from
    JavaScript comes out at about 40 MB, 28 MB of which is ICU's data.
  - On the two-core Miyoo, set these before starting the program (Playload's launch.sh does):
        export JSC_numberOfDFGCompilerThreads=1 JSC_numberOfGCMarkers=1
  - A script starts in the interpreter and gets compiled as it warms up, so the first second is the slowest.
  - Measured with Playload on a Miyoo Mini Plus at 1600 MHz: compiled code runs about 6-8 times slower than
    on a desktop PC; the QuickJS interpreter was 12-15 times slower on the same games.
  - Calls between C and JavaScript are the expensive part. Pass typed arrays once and share their memory
    (JSObjectGetTypedArrayBytesPtr) rather than calling across for every pixel or number.

License
  JavaScriptCore and WTF are LGPL 2.1 (parts BSD). Linking them statically into something you publish means
  shipping the license and letting people relink against their own build of the library: the simplest way is to
  publish your program's source or object files along with it. ICU is under the Unicode license. This is a
  summary, not legal advice.

```

Built for [Playload for Miyoo](https://kitao.dev/playload/) by 0kCyb0rg.
