#################### rtmp_pub #######################

include_directories(librtmp/dist/android/include
        x264/include
        x265/include)
link_directories(librtmp/dist/android/lib
        x264/jniLibs
        x265/jniLibs)

include_directories(
        live555/include
        src/john_collections)

set(rtmp_pub_src
        src/stream_media/sps_decode.h
        src/stream_media/x264_stream.c
        src/stream_media/x265_stream.c
        src/john_collections/john_queue.c
        src/john_collections/john_synchronized_queue.c
        src/stream_media/trace_malloc_free.c
        src/stream_media/rtmp_publisher.c)
find_library(log-lib log)
add_library(rtmp-pub SHARED ${rtmp_pub_src})
target_link_libraries(rtmp-pub ${log-lib} x264.152 x265 rtmp)