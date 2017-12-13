Build For Android: [2.6]
#remove pthread, "-lpthread" in CMakeLists.txt
#pick #1 or #2

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
-G "Android Gradle - Ninja" \
../source
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
-G "Android Gradle - Unix Makefiles" \
../source
make all && make install

Build For Mac OS X: [2.6]
cmake \
-DCMAKE_INSTALL_PREFIX=dist \
-DCMAKE_BUILD_TYPE=Debug \
-G "Unix Makefiles" \
../source
make && make install
