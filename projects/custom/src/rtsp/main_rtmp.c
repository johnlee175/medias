#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "common.h"
#include "trace_malloc_free.h"
#include "rtmp_publisher.h"

bool quit = false;

void *rtmp_source(void *client) {
    LOGW("rtmp_source\n");
    FILE *file = fopen("medias/projects/custom/data/test.264", "r");

    size_t buffer_size = 1024; /* buffer_size larger, movie faster */
    uint8_t *buffer = malloc(buffer_size);

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

void *rtmp_produce(void *client) {
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