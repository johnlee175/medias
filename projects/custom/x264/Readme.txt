Build For Android: [x264-20171115-2245 build-id=152]

see configure relace "libx264.so.$API" => "libx264.$API.so"

./configure \
--prefix=`pwd`/dist \
--enable-shared \
--disable-cli \
--disable-win32thread \
--enable-strip \
--host=arm-linux \
--cross-prefix=${ANDROID_NDK_HOME}/toolchains/arm-linux-androideabi-4.9/prebuilt/darwin-x86_64/bin/arm-linux-androideabi- \
--sysroot=${ANDROID_NDK_HOME}/platforms/android-19/arch-arm

make && make install

Usages: -lm -ldl -lx264

Build For Mac OS X:
#0. On Mac: otool -L <dylib file>
#1. On Mac: install_name_tool -id @executable_path/<new dylib relative-path> <dylib file>
#2. On Mac: install_name_tool -id @rpath/<new dylib relative-path> <dylib file>
#2. On Mac: install_name_tool -add_rpath <your rpath> <executable file>
