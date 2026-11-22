#!/bin/sh
# Cross-compile hello.c for the Miyoo Mini / Mini Plus.   TC=/path/to/miyoomini-toolchain ./build.sh
set -e
cd "$(dirname "$0")"
TC=${TC:-$HOME/miyoomini-toolchain}
K=..
ARCH="-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard"
$TC/bin/arm-linux-gnueabihf-gcc $ARCH -marm -O2 -I$K/include -c hello.c -o hello.o
# link with g++: JavaScriptCore is C++ inside. Order matters: JavaScriptCore, WTF, then ICU (i18n, uc, data).
$TC/bin/arm-linux-gnueabihf-g++ $ARCH -o hello hello.o \
  -Wl,--start-group $K/lib/libJavaScriptCore.a $K/lib/libWTF.a -Wl,--end-group \
  $K/lib/libicui18n.a $K/lib/libicuuc.a $K/lib/libicudata.a \
  -static-libstdc++ -static-libgcc -Wl,-Bstatic -lstdc++fs -latomic -Wl,-Bdynamic -lpthread -ldl -lm -Wl,--gc-sections
$TC/bin/arm-linux-gnueabihf-strip hello
ls -la hello
