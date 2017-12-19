#Build libvpx [https://chromium.googlesource.com/webm/libvpx/+/v1.6.1]

#On Mac OS X [Sierra 10.12.6 darwin16]
./configure --prefix=dist --target=x86_64-darwin15-gcc --enable-shared --enable-static
make -j2 && make install


#On Android [NDK Version 16] (see NDK-HOME/source.properties)
if [ -d ${ANDROID_HOME}/ndk-bundle/sysroot ]; then
ADD_FLAGS="-I${ANDROID_HOME}/ndk-bundle/sysroot/usr/include -I${ANDROID_HOME}/ndk-bundle/sysroot/usr/include/arm-linux-androideabi"
fi
./configure \
--prefix=dist \
--target=armv7-android-gcc \
--sdk-path=${ANDROID_HOME} \
--disable-runtime-cpu-detect \
--disable-webm-io \
--extra-cflags="${ADD_FLAGS} -mfloat-abi=softfp -mfpu=neon" \
--extra-cxxflags="-I${ANDROID_HOME}/ndk-bundle/sysroot/usr/include -I${ANDROID_HOME}/ndk-bundle/sysroot/usr/include/arm-linux-androideabi -mfloat-abi=softfp -mfpu=neon"
make -j2 && make install
