#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "common.h"
#include "x264_stream.h"
#include "rtmp_publisher.h"
#include "trace_malloc_free.h"

/* #define rtmp_source rtmp_file_source /* -- */
#define rtmp_source rtmp_x264_source /* -- */

static bool quit = false;

static void *rtmp_file_source(void *client) {
    LOGW("rtmp_file_source\n");
    FILE *file = fopen("medias/projects/custom/data/test.264", "r");

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

static void *rtmp_x264_source(void *client) {
    LOGW("rtmp_x264_source\n");
    /* ffmpeg -i data/cuc_ieschool.mp4 -c:v rawvideo -pix_fmt yuv420p data/cuc_ieschool.yuv */
    FILE *file = fopen("medias/projects/custom/data/cuc_ieschool.yuv", "r");

    const int width = 512, height = 288;
    size_t frame_size = width * height * 3 / 2;
    uint8_t *frame = (uint8_t *) malloc(frame_size);

    if (!file || !frame) {
        LOGW("rtmp_source prepare failed!\n");
        quit = true;
        return NULL;
    }

    X264Stream *stream = NULL;
    if (!(stream = create_x264_module(width, height, -1, NULL, NULL, NULL,
                                      NULL, on_encoded_frame, client))) {
        LOGW("create_x264_module failed!\n");
        quit = true;
        return NULL;
    }

    while (!quit) {
        if (fread(frame, 1, frame_size, file) <= 0) {
            break;
        }
        if (append_i420_frame(stream, frame) < 0) {
            LOGW("append_i420_frame failed!\n");
            break;
        }
        LOGW("source update...\n");
        usleep(10000);
    }

    if (encode_x264_frame(stream) < 0) {
        LOGW("encode_x264_frame failed!\n");
    }
    destroy_x264_module(stream);

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
    rtmp_publisher_avc_consume_loop(publisher, &quit);

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