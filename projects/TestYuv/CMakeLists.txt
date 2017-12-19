cmake_minimum_required(VERSION 3.4.1)
set (CMAKE_C_STANDARD 99)
set (CMAKE_CXX_STANDARD 11)

add_definitions(-DOutPacketBufferMaxSize=240000)
add_definitions(-DALLOW_SERVER_PORT_REUSE)

include_directories(
            src/main/cpp/include/
            src/main/cpp/include/x264
            src/main/cpp/include/x265
            src/main/cpp/include/live555
            src/main/cpp/include/rtmp )

add_library( rtsp-stream SHARED
             src/main/cpp/main_server.cpp )
add_library( rtmp-stream SHARED
             src/main/cpp/main_publisher.c )

add_library( x264 SHARED IMPORTED )
set_target_properties( x264 PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libx264.152.so )
add_library( x265 SHARED IMPORTED )
set_target_properties( x265 PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libx265.so )
add_library( live555 SHARED IMPORTED )
set_target_properties( live555 PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/liblive555.so )
add_library( rtmp SHARED IMPORTED )
set_target_properties( rtmp PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/librtmp.1.so )
add_library( rtmp-pub SHARED IMPORTED )
set_target_properties( rtmp-pub PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/librtmp-pub.so )

add_library( vpx STATIC IMPORTED )
set_target_properties( vpx PROPERTIES IMPORTED_LOCATION
                       ${CMAKE_SOURCE_DIR}/src/main/archives/armeabi/libvpx.a )

find_library( log-lib log )

target_link_libraries( rtsp-stream
                       x264 x265 vpx live555 ${log-lib} )

target_link_libraries( rtmp-stream
                       x264 rtmp rtmp-pub ${log-lib} )
