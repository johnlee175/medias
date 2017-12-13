#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "common.h"
#include "base_path.h"
#include "x264_stream.h"
#include "x265_stream.h"
#include "rtmp_publisher.h"
#include "trace_malloc_free.h"

#define H26X_KEY 265
#define H26X_KEY_TO_STR(key) #key
#define H26X_STR_EVAL(key) H26X_KEY_TO_STR(key)
#define H26X_STR H26X_STR_EVAL(H26X_KEY)

#define H26X_JOIN(prefix, key, suffix) prefix##key##suffix
#define H26X_JOIN_EVAL(prefix, key, suffix) H26X_JOIN(prefix, key,suffix)
#define H26X_FUNC(prefix, suffix) H26X_JOIN_EVAL(prefix, H26X_KEY, suffix)
#define H26X_STRUCT(prefix, suffix) H26X_JOIN_EVAL(prefix, H26X_KEY, suffix)

#define rtmp_source rtmp_file_source /* -- */
/* #define rtmp_source rtmp_x26x_source /* -- */

static bool quit = false;

static void *rtmp_file_source(void *client) {
    LOGW("rtmp_file_source\n");
    FILE *file = fopen(BASE_PATH"/data/test."H26X_STR, "r");

    size_t buffer_size = 1024; /* buffer_size larger, movie faster */
    uint8_t *buffer = (uint8_t *) malloc(buffer_size);

    if (!file || !buffer) {
        LOGW("rtmp_source prepare failed!\n");
        quit = true;
        return NULL;
    }

    size_t read_count;
    while (!quit) {
        if ((read_count = fread(buffer, 1, buffer_size, file)) <= 0) {
            break;
        }
        if (!rtmp_publisher_update_source((RtmpPublisher *) client, buffer, (uint32_t) read_count)) {
            LOGW("rtmp_publisher_update_source failed!\n");
            break;
        }
        LOGW("source update...\n");
        usleep(10000);
    }

    fclose(file);
    free(buffer);
    LOGW("source close\n");
    usleep(1000000);
    quit = true;
    return NULL;
}

static void on_encoded_frame(uint8_t *payload, uint32_t size, void *client) {
    if (!rtmp_publisher_update_source((RtmpPublisher *) client, payload, size)) {
        LOGW("rtmp_publisher_update_source failed!\n");
    }
}

static void *rtmp_x26x_source(void *client) {
    LOGW("rtmp_x26x_source\n");
    /* ffmpeg -i data/test.mp4 -c:v rawvideo -pix_fmt yuv420p data/test_512x288.yuv */
    FILE *file = fopen(BASE_PATH"/data/test_512x288.yuv", "r");

    const int width = 512, height = 288;
    size_t frame_size = width * height * 3 / 2;
    uint8_t *frame = (uint8_t *) malloc(frame_size);

    if (!file || !frame) {
        LOGW("rtmp_source prepare failed!\n");
        quit = true;
        return NULL;
    }

    H26X_STRUCT(X,Stream) *stream = NULL;
    if (!(stream = H26X_FUNC(create_x, _module)(width, height, -1, NULL, NULL, NULL,
                                      NULL, on_encoded_frame, client))) {
        LOGW("create_x%d_module failed!\n", H26X_KEY);
        quit = true;
        return NULL;
    }

    while (!quit) {
        if (fread(frame, 1, frame_size, file) <= 0) {
            break;
        }
        if (H26X_FUNC(append_yuv_frame_x, )(stream, frame) < 0) {
            LOGW("append_yuv_frame_x%d failed!\n", H26X_KEY);
            break;
        }
        LOGW("source update...\n");
        usleep(10000);
    }

    if (H26X_FUNC(encode_x, _frame)(stream) < 0) {
        LOGW("encode_x%d_frame failed!\n", H26X_KEY);
    }
    H26X_FUNC(destroy_x, _module)(stream);

    fclose(file);
    free(frame);
    LOGW("source close\n");

    usleep(1000000);
    quit = true;
    return NULL;
}

static void *rtmp_produce(void *client) {
    LOGW("rtmp_produce\n");
    rtmp_publisher_avc_produce_loop((RtmpPublisher *) client, &quit);
    return NULL;
}

int main() {
    trace_malloc_free_create();

    RtmpPublisher *publisher = rtmp_publisher_create("rtmp://localhost/live", NULL);
    if (!publisher) {
        LOGW("rtmp_publisher_create failed!\n");
        return -1;
    }

    pthread_t produce_thread;
    if (pthread_create(&produce_thread, NULL, rtmp_produce, publisher)) {
        LOGW("pthread_create failed!\n");
        return -1;
    }

    usleep(20000);

    pthread_t source_thread;
    if (pthread_create(&source_thread, NULL, rtmp_source, publisher)) {
        LOGW("pthread_create failed!\n");
        return -1;
    }

    usleep(20000);

    LOGW("rtmp_consume\n");
    rtmp_publisher_avc_consume_loop(publisher, &quit, H26X_KEY);

    LOGW("clean and exit...\n");
    quit = true;

    pthread_join(source_thread, NULL);
    LOGW("source thread exit...\n");
    pthread_join(produce_thread, NULL);
    LOGW("produce thread exit...\n");

    rtmp_publisher_destroy(publisher);
    trace_malloc_free_destroy();
    return 0;
}