#################### john_collections #######################

include_directories(src/john_collections)
add_library(john_collections SHARED
        src/john_collections/john_stack.c
        src/john_collections/john_queue.c
        src/john_collections/john_ring_buffer.c
        src/john_collections/john_object_pool.c
        src/john_collections/john_synchronized_queue.c
        src/john_collections/john_sync_ring_buffer.c)

#################### important-dependencies #######################

include_directories(live555/include) # temp apply for common.h

set(opencv_include_files opencv/include opencv/include_${PLATFORM_SUBDIR}
        opencv/include/opencv opencv/include/opencv2
        opencv/include_${PLATFORM_SUBDIR}/opencv2)
set(sdl_include_files sdl/include sdl/include_${PLATFORM_SUBDIR}
        sdl/include/SDL2 sdl/include_${PLATFORM_SUBDIR}/SDL2)

include_directories(
        ffmpeg/include
        ${opencv_include_files}
        ${sdl_include_files})

link_directories(
        ffmpeg/lib/${PLATFORM_SUBDIR}
        opencv/lib/${PLATFORM_SUBDIR}
        sdl/lib/${PLATFORM_SUBDIR})

set(ffmpeg_libs
        avformat
        avcodec
        avutil
        swscale
        swresample
        avdevice
        avfilter
        postproc)

set(opencv_libs
        opencv_core
        opencv_highgui
        opencv_imgcodecs
        opencv_imgproc)

#################### ff_trans_enc_client #######################

add_executable(ff_trans_enc_client
        src/stream_media/ff_trans_enc_client.c)
target_link_libraries(ff_trans_enc_client ${ffmpeg_libs})

#################### base_media_client #######################

add_executable(base_media_client
        src/stream_media/trace_malloc_free.c
        src/stream_media/ffmpeg_base_client.h
        src/stream_media/ffmpeg_base_client.c)
target_link_libraries(base_media_client ${ffmpeg_libs} john_collections SDL2)
