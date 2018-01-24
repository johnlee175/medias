Build For Android: [2.0.7]
#pick #1 or #2 or #3

rm -rf build && mkdir build && cd build
CMAKE_VER=3.6.4111459

#1:
${ANDROID_HOME}/cmake/${CMAKE_VER}/bin/cmake \
-DANDROID_ABI=armeabi \
-DANDROID_NDK=${ANDROID_HOME}/ndk-bundle \
-DCMAKE_INSTALL_PREFIX=dist \
-DCMAKE_BUILD_TYPE=Debug \
-DCMAKE_MAKE_PROGRAM=${ANDROID_HOME}/cmake/${CMAKE_VER}/bin/ninja \
-DCMAKE_TOOLCHAIN_FILE=${ANDROID_HOME}/ndk-bundle/build/cmake/android.toolchain.cmake \
-DANDROID_PLATFORM=android-16 \
-DANDROID_NATIVE_API_LEVEL=16 \
-DANDROID_STL=c++_static \
-DANDROID_DEPRECATED_HEADERS=ON \
-DANDROID_ARM_NEON=TRUE \
-DANDROID_TOOLCHAIN=clang \
-DANDROID_PIE=ON \
-DANDROID_ARM_MODE=arm \
-G "Android Gradle - Ninja" ..
${ANDROID_HOME}/cmake/${CMAKE_VER}/bin/ninja all
${ANDROID_HOME}/cmake/${CMAKE_VER}/bin/ninja install

#2
${ANDROID_HOME}/cmake/${CMAKE_VER}/bin/cmake \
-DANDROID_ABI=armeabi \
-DANDROID_NDK=${ANDROID_HOME}/ndk-bundle \
-DCMAKE_INSTALL_PREFIX=dist \
-DCMAKE_BUILD_TYPE=Debug \
-DCMAKE_TOOLCHAIN_FILE=${ANDROID_HOME}/ndk-bundle/build/cmake/android.toolchain.cmake \
-DANDROID_PLATFORM=android-16 \
-DANDROID_NATIVE_API_LEVEL=16 \
-DANDROID_STL=c++_static \
-DANDROID_DEPRECATED_HEADERS=ON \
-DANDROID_ARM_NEON=TRUE \
-DANDROID_TOOLCHAIN=clang \
-DANDROID_PIE=ON \
-DANDROID_ARM_MODE=arm \
-G "Android Gradle - Unix Makefiles" ..
make all && make install

#3 [ndk-build]
./build-scripts/androidbuildlibs.sh

Build For Mac OS X / Linux: [2.0.7]
CC=`pwd`/build-scripts/gcc-fat.sh
rm -rf build && mkdir build && cd build
../configure --prefix=`pwd`/dist && make -j4 && make install

Different platforms include directory, only the SDL_config.h file is different, others are some.
