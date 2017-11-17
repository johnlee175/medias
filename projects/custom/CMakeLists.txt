cmake_minimum_required(VERSION 3.8)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)

add_definitions(-DALLOW_SERVER_PORT_REUSE)

include_directories(
        x264/include
        ffmpeg/include
        opencv/include
        live555/include
        live555/include/BasicUsageEnvironment
        live555/include/groupsock
        live555/include/liveMedia
        live555/include/UsageEnvironment)

link_directories(
        x264/lib
        ffmpeg/lib
        opencv/lib
        live555/lib)

set(live555_libs
        UsageEnvironment
        BasicUsageEnvironment
        groupsock
        liveMedia)

set(ffmpeg_libs
        avformat
        avcodec
        avutil
        swscale
        avdevice
        avfilter
        swresample)

set(opencv_libs
        opencv_core
        opencv_highgui
        opencv_imgcodecs
        opencv_imgproc)

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
        src/rtsp/x264_stream.c
        src/rtsp/rtsp_ffmpeg_client.c
        src/rtsp/ExchangerDeviceSource.cpp src/rtsp/ExchangerH264VideoServerMediaSubsession.cpp
        src/rtsp/ExchangerH264VideoServer.hpp src/rtsp/common.h)

set(rtsp_depend x264
        ${live555_libs} ${ffmpeg_libs} ${opencv_libs})

add_executable(hello_rtsp_server ${hello_rtsp_code} src/rtsp/main_server.cpp)
add_executable(hello_rtsp_client ${hello_rtsp_code} src/rtsp/main_client.cpp)
target_link_libraries(hello_rtsp_server ${rtsp_depend})
target_link_libraries(hello_rtsp_client ${rtsp_depend})
