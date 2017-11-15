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
#include <errno.h>
#include <stdlib.h>
#include <memory.h>
#include <AppleTextureEncoder.h>
#include "john_queue.h"
#include "john_synchronized_queue.h"

struct JohnSynchronizedQueue {
    JohnQueue *queue;
    bool replace_oldest;
    void *return_on_empty;
    pthread_mutex_t lock;
    pthread_cond_t empty_condition;
    pthread_cond_t full_condition;
};

JohnSynchronizedQueue *john_synchronized_queue_create(uint32_t capacity,
                                                      bool replace_oldest, void *return_on_empty) {
    JohnSynchronizedQueue *synchronized_queue = (JohnSynchronizedQueue *) malloc(sizeof(JohnSynchronizedQueue));
    if (synchronized_queue) {
        memset(synchronized_queue, 0, sizeof(JohnSynchronizedQueue));
        synchronized_queue->queue = john_queue_create(capacity);
        synchronized_queue->replace_oldest = replace_oldest;
        synchronized_queue->return_on_empty = return_on_empty;

        pthread_mutex_init(&synchronized_queue->lock, NULL);
        pthread_cond_init(&synchronized_queue->empty_condition, NULL);
        pthread_cond_init(&synchronized_queue->full_condition, NULL);
    }
    return synchronized_queue;
}

void john_synchronized_queue_destroy(JohnSynchronizedQueue *synchronized_queue) {
    if (synchronized_queue) {
        pthread_cond_destroy(&synchronized_queue->empty_condition);
        memset(&synchronized_queue->empty_condition, 0, sizeof(pthread_cond_t));
        pthread_cond_destroy(&synchronized_queue->full_condition);
        memset(&synchronized_queue->full_condition, 0, sizeof(pthread_cond_t));
        pthread_mutex_destroy(&synchronized_queue->lock);
        memset(&synchronized_queue->lock, 0, sizeof(pthread_mutex_t));

        john_queue_destroy(synchronized_queue->queue);
        synchronized_queue->queue = NULL;
        free(synchronized_queue);
    }
}

bool john_synchronized_queue_enqueue(JohnSynchronizedQueue *synchronized_queue, void *data, int32_t timeout_millis) {
    bool result = false;
    if (!synchronized_queue || !data) {
        return result;
    }
    pthread_mutex_lock(&synchronized_queue->lock);
    if (!john_queue_is_full(synchronized_queue->queue)) {
        goto success;
    } else if (timeout_millis == 0) {
        goto check;
    } else if (timeout_millis < 0) {
        while (true) {
            pthread_cond_wait(&synchronized_queue->full_condition, &synchronized_queue->lock);
            if (!john_queue_is_full(synchronized_queue->queue)) {
                goto success;
            }
        }
    } else { /* timeout_millis > 0 */
        int rc;
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        now.tv_sec += timeout_millis / 1000;
        now.tv_nsec += (timeout_millis % 1000) * 1000000;
        while (true) {
            rc = pthread_cond_timedwait(&synchronized_queue->full_condition, &synchronized_queue->lock, &now);
            if (!john_queue_is_full(synchronized_queue->queue)) {
                goto success;
            } else if (rc == ETIMEDOUT) {
                goto check;
            }
        }
    }
    check:
    if (!synchronized_queue->replace_oldest) {
        goto fail;
    } else {
        john_queue_dequeue(synchronized_queue->queue);
        pthread_cond_signal(&synchronized_queue->full_condition);
        if (!john_queue_is_full(synchronized_queue->queue)) {
            goto success;
        } else {
            goto fail;
        }
    }
    success:
    result = john_queue_enqueue(synchronized_queue->queue, data);
    pthread_cond_signal(&synchronized_queue->empty_condition);
    pthread_mutex_unlock(&synchronized_queue->lock);
    return result;
    fail:
    pthread_mutex_unlock(&synchronized_queue->lock);
    return result;
}

void *john_synchronized_queue_dequeue(JohnSynchronizedQueue *synchronized_queue, int32_t timeout_millis) {
    void *result = NULL;
    if (!synchronized_queue) {
        return result;
    }
    result = synchronized_queue->return_on_empty;
    pthread_mutex_lock(&synchronized_queue->lock);
    if (!john_queue_is_empty(synchronized_queue->queue)) {
        goto success;
    } else if (timeout_millis == 0) {
        goto fail;
    } else if (timeout_millis < 0) {
        while (true) {
            pthread_cond_wait(&synchronized_queue->empty_condition, &synchronized_queue->lock);
            if (!john_queue_is_empty(synchronized_queue->queue)) {
                goto success;
            }
        }
    } else { /* timeout_millis > 0 */
        int rc = 0;
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        now.tv_sec += timeout_millis / 1000;
        now.tv_nsec += (timeout_millis % 1000) * 1000000;
        while (true) {
            rc = pthread_cond_timedwait(&synchronized_queue->empty_condition, &synchronized_queue->lock, &now);
            if (!john_queue_is_empty(synchronized_queue->queue)) {
                goto success;
            } else if (rc == ETIMEDOUT) {
                goto fail;
            }
        }
    }
    success:
    result = john_queue_dequeue(synchronized_queue->queue);
    pthread_cond_signal(&synchronized_queue->full_condition);
    pthread_mutex_unlock(&synchronized_queue->lock);
    return result;
    fail:
    pthread_mutex_unlock(&synchronized_queue->lock);
    return result;
}

void *john_synchronized_queue_head(JohnSynchronizedQueue *synchronized_queue) {
    void *result = NULL;
    if (synchronized_queue) {
        result = synchronized_queue->return_on_empty;
        pthread_mutex_lock(&synchronized_queue->lock);
        void *head = john_queue_head(synchronized_queue->queue);
        if (head) {
            result = head;
        }
        pthread_mutex_unlock(&synchronized_queue->lock);
    }
    return result;
}

void *john_synchronized_queue_tail(JohnSynchronizedQueue *synchronized_queue) {
    void *result = NULL;
    if (synchronized_queue) {
        result = synchronized_queue->return_on_empty;
        pthread_mutex_lock(&synchronized_queue->lock);
        void *tail = john_queue_tail(synchronized_queue->queue);
        if (tail) {
            result = tail;
        }
        pthread_mutex_unlock(&synchronized_queue->lock);
    }
    return result;
}

bool john_synchronized_queue_is_full(JohnSynchronizedQueue *synchronized_queue) {
    bool result = false;
    if (synchronized_queue) {
        pthread_mutex_lock(&synchronized_queue->lock);
        result = john_queue_is_full(synchronized_queue->queue);
        pthread_mutex_unlock(&synchronized_queue->lock);
    }
    return result;
}

bool john_synchronized_queue_is_empty(JohnSynchronizedQueue *synchronized_queue) {
    bool result = false;
    if (synchronized_queue) {
        pthread_mutex_lock(&synchronized_queue->lock);
        result = john_queue_is_empty(synchronized_queue->queue);
        pthread_mutex_unlock(&synchronized_queue->lock);
    }
    return result;
}

void john_synchronized_queue_clear(JohnSynchronizedQueue *synchronized_queue) {
    if (synchronized_queue) {
        pthread_mutex_lock(&synchronized_queue->lock);
        john_queue_clear(synchronized_queue->queue);
        pthread_mutex_unlock(&synchronized_queue->lock);
    }
}
