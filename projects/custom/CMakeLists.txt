cmake_minimum_required(VERSION 3.8)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)

add_definitions(-DOutPacketBufferMaxSize=240000)
add_definitions(-DALLOW_SERVER_PORT_REUSE)

include_directories(
        src/john_collections
        x264/include
        ffmpeg/include
        opencv/include
        live555/include
        librtmp/dist/macosx/include)

link_directories(
        x264/lib
        ffmpeg/lib
        opencv/lib
        live555/lib
        librtmp/dist/macosx/lib)

set(ffmpeg_libs
        avformat
        avcodec
        avutil
        swscale
        avdevice
        avfilter
        swresample)

set(opencv_lib_version 3.3.0)
set(opencv_libs
        opencv_core.${opencv_lib_version}
        opencv_highgui.${opencv_lib_version}
        opencv_imgcodecs.${opencv_lib_version}
        opencv_imgproc.${opencv_lib_version})

#################### john_collections #######################

add_library(john_collections SHARED
        src/john_collections/john_stack.c
        src/john_collections/john_queue.c
        src/john_collections/john_ring_buffer.c
        src/john_collections/john_object_pool.c
        src/john_collections/john_synchronized_queue.c
        src/john_collections/john_sync_ring_buffer.c)

################### hello_udp ########################

set(hello_udp_code src/udp/udp_trans.h src/udp/udp_trans.c src/udp/main.cpp)
add_executable(hello_udp ${hello_udp_code})
target_link_libraries(hello_udp ${opencv_libs})

#################### hello_rtsp #######################

set(hello_rtsp_code
        src/rtsp/sps_decode.h
        src/rtsp/x264_stream.c
        src/rtsp/trace_malloc_free.c
        src/rtsp/rtmp_publisher.c
        src/rtsp/rtsp_ffmpeg_client.c)

set(rtsp_depend john_collections x264.152 live555 rtmp ${ffmpeg_libs} ${opencv_libs})

add_executable(hello_rtsp_server ${hello_rtsp_code} src/rtsp/main_server.cpp)
add_executable(hello_rtsp_client ${hello_rtsp_code} src/rtsp/main_client.cpp)
add_executable(hello_as_rtmp ${hello_rtsp_code} src/rtsp/main_rtmp.c)
target_link_libraries(hello_rtsp_server ${rtsp_depend})
target_link_libraries(hello_rtsp_client ${rtsp_depend})
target_link_libraries(hello_as_rtmp ${rtsp_depend})
