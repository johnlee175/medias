#!/usr/bin/env bash

INSTALL_VIDEOIO_MODULE="MAIN" #"MAIN", "QT", others
SYS_DESC=`uname -a`

if [[ ${SYS_DESC} == *Ubuntu* ]]; then
    echo -e "we are on Ubuntu"
    echo -e "sudo apt-get install yasm libtool automake build-essential git pkg-config wget \
    libncurses5 qt5-default doxygen"
    sudo apt-get install yasm libtool autoconf automake build-essential git pkg-config wget \
    libncurses5 qt5-default doxygen
    CMAKE_DEPS_STR="--qt-gui --enable-ccache"

    if [ ${INSTALL_VIDEOIO_MODULE} == "QT" ]; then
        echo -e "we use qt/qtkit/qt-opengl instread ffmpeg for opencv-videoio on Ubuntu.
        make sure sudo apt-get install qt5-default had executed."
        export CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}":/usr/lib/x86_64-linux-gnu/cmake
        VIDEOIO_OPTS="-DWITH_OPENGL=ON -DWITH_QT=ON -DWITH_QTKIT=ON -DWITH_GSTREAMER=OFF -DWITH_GTK=OFF -DWITH_GTK_2_X=OFF -DWITH_FFMPEG=OFF"
    elif [ ${INSTALL_VIDEOIO_MODULE} == "GTK" ]; then
        echo -e "we use gtk/gstreamer/gtkglext(OpenGL) instead ffmpeg for opencv-videoio on Ubuntu.
        because opencv need high version ffmpeg, but libav or ffmpeg module though apt-get installed is low version,
        you have to build ffmpeg manually and link it to opencv."
        echo -e "sudo apt-get install libgstreamer1.0-0 libgstreamer1.0-dev gstreamer1.0-tools \
        libgstreamer-plugins-base1.0-0 libgstreamer-plugins-base1.0-dev \
        gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly gstreamer1.0-plugins-bad \
        libgtk2.0-0 libgtk2.0-dev libgtkglext1 libgtkglext1-dev"
        sudo apt-get install libgstreamer1.0-0 libgstreamer1.0-dev gstreamer1.0-tools \
        libgstreamer-plugins-base1.0-0 libgstreamer-plugins-base1.0-dev \
        gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly gstreamer1.0-plugins-bad \
        libgtk2.0-0 libgtk2.0-dev libgtkglext1 libgtkglext1-dev
        VIDEOIO_OPTS="-DWITH_OPENGL=ON -DWITH_QT=OFF -DWITH_QTKIT=OFF -DWITH_GSTREAMER=ON -DWITH_GTK=ON -DWITH_GTK_2_X=ON -DWITH_FFMPEG=OFF"
    else
        echo -e "we disable opencv-videoio because of it depends on gstreamer or ffmpeg"
        VIDEOIO_OPTS="-DBUILD_opencv_videoio=OFF"
    fi

elif [[ ${SYS_DESC} == *Darwin* ]]; then
    echo -e "we are on Mac OS X"
    echo -e "brew install yasm libtool automake wget shtool \
    ncurses qt5 doxygen"
    brew install yasm libtool automake wget shtool \
    ncurses qt5 doxygen
    CMAKE_DEPS_STR=""

    if [ ${INSTALL_VIDEOIO_MODULE} == "QT" ]; then
        echo -e "we use qt/qtkit/qt-opengl instread ffmpeg for opencv-videoio on Mac OS X.
        make sure brew install qt5 had executed.
        Warn: maybe will build failed with QTKit.h not found."
        export CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}":/usr/lib/x86_64-linux-gnu/cmake
        VIDEOIO_OPTS="-DWITH_OPENGL=ON -DWITH_QT=ON -DWITH_QTKIT=ON -DWITH_AVFOUNDATION=OFF -DWITH_FFMPEG=OFF"
    elif [ ${INSTALL_VIDEOIO_MODULE} == "MAIN" ]; then
        echo -e "we use Cocoa/AVFoundation instead ffmpeg for opencv-videoio on Mac OS X"
        VIDEOIO_OPTS="-DWITH_OPENGL=ON -DWITH_QT=OFF -DWITH_QTKIT=OFF -DWITH_AVFOUNDATION=ON -DWITH_FFMPEG=OFF"
    else
        echo -e "we disable opencv-videoio"
        VIDEOIO_OPTS="-DBUILD_opencv_videoio=OFF"
    fi
fi

#make sure the package has cmake, ccmake, cmake-gui, ccache installed
mkdir tmp && cd tmp
wget https://cmake.org/files/v3.10/cmake-3.10.2.tar.gz && tar zxf cmake-3.10.2.tar.gz && cd cmake-3.10.2
 ./configure ${CMAKE_DEPS_STR} && make -j4 && sudo make install
cd ../../ && rm -rf tmp

echo -e "build and install opencv..."
mkdir tmp && cd tmp
wget https://github.com/opencv/opencv/archive/3.3.0.zip && unzip 3.3.0.zip && cd opencv-3.3.0
mkdir build && cd build
#you can use "ccmake .." here to custom opencv build option configuration, we use command line here.
cmake -DCMAKE_INSTALL_PREFIX=dist -DCMAKE_BUILD_TYPE=Release -DCMAKE_VERBOSE_MAKEFILE=ON ${VIDEOIO_OPTS} ..
