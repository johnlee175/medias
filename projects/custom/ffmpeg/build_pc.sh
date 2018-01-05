#!/usr/bin/env bash

if [ $# -ne 1 ]; then
    echo "need path parameter which locate ffmpeg home"
    exit
fi
cd $1
echo "We are in $1"

MODE="release" #debug or release
EXT_MODE="NO" #YES or NO

if [ $EXT_MODE == "YES" ]; then
	SYS_DESC=`uname -a`
	if [[ ${SYS_DESC} == *Ubuntu* ]]; then
	    echo "we are on Ubuntu"
	    echo -e "sudo apt-get install yasm libtool autoconf automake build-essential cmake git pkg-config wget texinfo \
	    libx264-dev libx265-dev libvpx-dev libfdk-aac-dev libmp3lame-dev libass-dev libfreetype6-dev \
	    libopus-dev libvorbis-dev libtheora-dev"
	    sudo apt-get install yasm libtool autoconf automake build-essential cmake git pkg-config wget texinfo \
	    libx264-dev libx265-dev libvpx-dev libfdk-aac-dev libmp3lame-dev libass-dev libfreetype6-dev \
	    libopus-dev libvorbis-dev libtheora-dev
	elif [[ ${SYS_DESC} == *Darwin* ]]; then
	    echo "we are on Mac OS X"
	    echo "brew install yasm libtool automake wget texi2html shtool \
	    x264 x265 libvpx fdk-aac lame libass opus libvorbis theora"
	    brew install yasm libtool automake wget texi2html shtool \
	    x264 x265 libvpx fdk-aac lame libass opus libvorbis theora
	fi
fi

BASE="--prefix=dist --enable-shared --enable-static --disable-sdl2"
LICENSE="--enable-gpl --enable-version3"

EXTEND=""
if [ $EXT_MODE == "YES" ]; then
EXTEND="\
  	--enable-libx264 \
  	--enable-libx265 \
  	--enable-libvpx \
	--enable-libfdk-aac \
  	--enable-libmp3lame \
	--enable-libass \
	--enable-libfreetype \
  	--enable-libopus \
  	--enable-libvorbis \
  	--enable-libtheora \
  	--enable-nonfree"
fi

rm -rf build && mkdir build && cd build

if [ $MODE == "debug" ]; then
echo "../configure $BASE $LICENSE $EXTEND --enable-debug=3 --disable-stripping --disable-optimizations \
--extra-cflags=\"-g3 -O0\" --extra-ldflags=\"-g3 -O0\""
../configure $BASE $LICENSE $EXTEND --enable-debug=3 --disable-stripping --disable-optimizations \
--extra-cflags="-g3 -O0" --extra-ldflags="-g3 -O0"
else
echo "../configure $BASE $LICENSE $EXTEND"
../configure $BASE $LICENSE $EXTEND
fi

echo "make and install ..."
make -j4 V=1

if [ $MODE == "debug" ]; then
make install
else
make install
fi

make clean
echo "Done!"