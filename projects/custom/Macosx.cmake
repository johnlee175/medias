include(UnixPC.cmake)

################### freq_opencv ########################

add_executable(freq_opencv src/others/freq_opencv.cpp)
target_link_libraries(freq_opencv ${opencv_libs})

################### pcm_proc_test ########################

#CMake Settings[environment variable]: export CMAKE_PREFIX_PATH=/usr/local/Cellar/qt/5.10.0_1/lib/cmake

find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Multimedia REQUIRED)
include_directories(${Qt5Core_INCLUDE_DIRS})
include_directories(${Qt5Widgets_INCLUDE_DIRS})
include_directories(${Qt5Multimedia_INCLUDE_DIRS})
set(QtLinkLibs ${Qt5Core_LIBRARIES} ${Qt5Widgets_LIBRARIES} ${Qt5Multimedia_LIBRARIES})

add_executable(pcm_proc_test src/others/pcm_proc_test.cpp)
target_link_libraries(pcm_proc_test ${QtLinkLibs})

################### hello_udp ########################

set(hello_udp_code src/udp/udp_trans.h src/udp/udp_trans.c src/udp/main.cpp)
add_executable(hello_udp ${hello_udp_code})
target_link_libraries(hello_udp ${opencv_libs})

#################### hello_stream_media #######################

include_directories(librtmp/dist/macosx/include
        live555/include
        x264/include
        x265/include
        libvpx/include)
link_directories(librtmp/dist/macosx/lib
        live555/lib
        x264/lib
        x265/lib
        libvpx/lib)

set(hello_stream_media
        src/stream_media/sps_decode.h
        src/stream_media/x264_stream.c
        src/stream_media/x265_stream.c
        src/stream_media/trace_malloc_free.c
        src/stream_media/rtmp_publisher.c
        src/stream_media/ffmpeg_url_client.c)

set(stream_media_depend john_collections x264.152 x265 vpx live555 rtmp ${ffmpeg_libs} ${opencv_libs})

add_executable(stream_media_client ${hello_stream_media} src/stream_media/main_client.cpp)
add_executable(hello_rtsp_server ${hello_stream_media} src/stream_media/main_rtsp_server.cpp)
add_executable(hello_rtmp_publisher ${hello_stream_media} src/stream_media/main_rtmp_publisher.c)
target_link_libraries(stream_media_client ${stream_media_depend})
target_link_libraries(hello_rtsp_server ${stream_media_depend})
target_link_libraries(hello_rtmp_publisher ${stream_media_depend})

add_executable(test_264 src/stream_media/x264_stream.c)
target_link_libraries(test_264 x264.152)
set_target_properties(test_264 PROPERTIES COMPILE_DEFINITIONS TEST_X264)

add_executable(test_265 src/stream_media/x265_stream.c)
target_link_libraries(test_265 x265)
set_target_properties(test_265 PROPERTIES COMPILE_DEFINITIONS TEST_X265)
