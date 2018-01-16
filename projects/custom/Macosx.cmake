set(CMAKE_BUILD_TYPE Debug)

include_directories(
        ffmpeg/include
        opencv/include)

link_directories(
        ffmpeg/lib
        opencv/lib)

set(ffmpeg_libs
        avformat
        avcodec
        avutil
        swscale
        swresample
        avdevice
        avfilter
        postproc)

set(opencv_lib_version 3.3.0)
set(opencv_libs
        opencv_core.${opencv_lib_version}
        opencv_highgui.${opencv_lib_version}
        opencv_imgcodecs.${opencv_lib_version}
        opencv_imgproc.${opencv_lib_version})

#################### john_collections #######################

include_directories(src/john_collections)
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

#################### base_media_client #######################

link_directories(sdl/lib)
add_executable(base_media_client
        src/stream_media/trace_malloc_free.c
        src/stream_media/ffmpeg_base_client.h
        src/stream_media/ffmpeg_base_client.c)
target_include_directories(base_media_client PRIVATE sdl/include)
target_link_libraries(base_media_client ${ffmpeg_libs} john_collections SDL2)

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
