/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements. See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache license, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the license for the specific language governing permissions and
 * limitations under the license.
 */
/**
 * @author John Kenrinus Lee
 * @version 2017-11-10
 */
#include <pthread.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include "john_ring_buffer.h"
#include "john_sync_ring_buffer.h"

struct JohnSyncRingBuffer {
    JohnRingBuffer *ring_buffer;
    void *return_on_empty;
    pthread_mutex_t lock;
    pthread_cond_t condition;
};

JohnSyncRingBuffer *john_sync_ring_buffer_create(uint32_t capacity, void *return_on_empty) {
    JohnSyncRingBuffer *ring_buffer = (JohnSyncRingBuffer *) malloc(sizeof(JohnSyncRingBuffer));
    if (ring_buffer) {
        memset(ring_buffer, 0, sizeof(JohnSyncRingBuffer));
        ring_buffer->ring_buffer = john_ring_buffer_create(capacity);
        ring_buffer->return_on_empty = return_on_empty;

        pthread_mutex_init(&ring_buffer->lock, NULL);
        pthread_cond_init(&ring_buffer->condition, NULL);
    }
    return ring_buffer;
}

void john_sync_ring_buffer_destroy(JohnSyncRingBuffer *ring_buffer) {
    if (ring_buffer) {
        pthread_cond_destroy(&ring_buffer->condition);
        memset(&ring_buffer->condition, 0, sizeof(pthread_cond_t));
        pthread_mutex_destroy(&ring_buffer->lock);
        memset(&ring_buffer->lock, 0, sizeof(pthread_mutex_t));

        john_ring_buffer_destroy(ring_buffer->ring_buffer);
        ring_buffer->ring_buffer = NULL;
        free(ring_buffer);
    }
}

void *john_sync_ring_buffer_read(JohnSyncRingBuffer *ring_buffer, int32_t timeout_millis) {
    void *result = NULL;
    if (!ring_buffer) {
        return result;
    }
    pthread_mutex_lock(&ring_buffer->lock);
    if ((result = john_ring_buffer_read(ring_buffer->ring_buffer, ring_buffer->return_on_empty)) !=
            ring_buffer->return_on_empty || timeout_millis == 0) {
        pthread_mutex_unlock(&ring_buffer->lock);
        return result;
    } else if (timeout_millis < 0) {
        while (true) {
            pthread_cond_wait(&ring_buffer->condition, &ring_buffer->lock);
            if ((result = john_ring_buffer_read(ring_buffer->ring_buffer, ring_buffer->return_on_empty)) !=
                    ring_buffer->return_on_empty) {
                pthread_mutex_unlock(&ring_buffer->lock);
                return result;
            }
        }
    } else { /* timeout_millis > 0 */
        int rc = 0;
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        now.tv_sec += timeout_millis / 1000;
        now.tv_nsec += (timeout_millis % 1000) * 1000000;
        if (now.tv_nsec > 1000000000) {
            now.tv_sec += 1;
            now.tv_nsec -= 1000000000;
        }
        while (true) {
            rc = pthread_cond_timedwait(&ring_buffer->condition, &ring_buffer->lock, &now);
            if ((result = john_ring_buffer_read(ring_buffer->ring_buffer, ring_buffer->return_on_empty)) !=
                ring_buffer->return_on_empty || rc == ETIMEDOUT) {
                pthread_mutex_unlock(&ring_buffer->lock);
                return result;
            }
        }
    }
}

void *john_sync_ring_buffer_write(JohnSyncRingBuffer *ring_buffer, void *data) {
    void *result = NULL;
    if (!ring_buffer) {
        return result;
    }
    result = ring_buffer->return_on_empty;
    if (data) {
        pthread_mutex_lock(&ring_buffer->lock);
        result = john_ring_buffer_write(ring_buffer->ring_buffer, data);
        pthread_cond_signal(&ring_buffer->condition);
        pthread_mutex_unlock(&ring_buffer->lock);
    }
    return result;
}

void john_sync_ring_buffer_clear(JohnSyncRingBuffer *ring_buffer) {
    if (ring_buffer) {
        pthread_mutex_lock(&ring_buffer->lock);
        john_ring_buffer_clear(ring_buffer->ring_buffer);
        pthread_mutex_unlock(&ring_buffer->lock);
    }
}
