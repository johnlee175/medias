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
#include <stdlib.h>
#include <memory.h>
#include "john_ring_buffer.h"

struct JohnRingBuffer {
    void **array;
    uint32_t capacity;
    int32_t read_pointer;
    int32_t write_pointer;
    int64_t round;
};

JohnRingBuffer *john_ring_buffer_create(uint32_t capacity) {
    JohnRingBuffer *ring_buffer = (JohnRingBuffer *) malloc(sizeof(JohnRingBuffer));
    if (ring_buffer) {
        memset(ring_buffer, 0, sizeof(JohnRingBuffer));
        ring_buffer->capacity = capacity;
        ring_buffer->read_pointer = 0;
        ring_buffer->write_pointer = 0;
        ring_buffer->round = 0;
        ring_buffer->array = (void **) malloc(sizeof(void *) * ring_buffer->capacity);
        memset(ring_buffer->array, 0, sizeof(void *) * ring_buffer->capacity);
    }
    return ring_buffer;
}

void john_ring_buffer_destroy(JohnRingBuffer *ring_buffer) {
    if (ring_buffer) {
        if (ring_buffer->array) {
            memset(ring_buffer->array, 0, sizeof(void *) * ring_buffer->capacity);
            free(ring_buffer->array);
        }
        memset(ring_buffer, 0, sizeof(JohnRingBuffer));
        free(ring_buffer);
    }
}

void *john_ring_buffer_read(JohnRingBuffer *ring_buffer, void *return_on_empty) {
    if (ring_buffer) {
        if (ring_buffer->read_pointer == ring_buffer->write_pointer && ring_buffer->round == 0) { /* is empty */
            return return_on_empty;
        }
        void *data = ring_buffer->array[ring_buffer->read_pointer];
        if (++ring_buffer->read_pointer >= ring_buffer->capacity) {
            --ring_buffer->round;
            ring_buffer->read_pointer = ring_buffer->read_pointer % ring_buffer->capacity;
        }
        return data;
    }
    return NULL;
}

void *john_ring_buffer_write(JohnRingBuffer *ring_buffer, void *data) {
    void *result = NULL;
    if (ring_buffer && data) {
        if (ring_buffer->write_pointer == ring_buffer->read_pointer && ring_buffer->round != 0) { /* is full */
            if (++ring_buffer->read_pointer >= ring_buffer->capacity) { /* ++read_pointer */
                --ring_buffer->round;
                ring_buffer->read_pointer = ring_buffer->read_pointer % ring_buffer->capacity;
            }
        }
        result = ring_buffer->array[ring_buffer->write_pointer];
        ring_buffer->array[ring_buffer->write_pointer] = data;
        if (++ring_buffer->write_pointer >= ring_buffer->capacity) {
            ++ring_buffer->round;
            ring_buffer->write_pointer = ring_buffer->write_pointer % ring_buffer->capacity;
        }
    }
    return result;
}

void john_ring_buffer_clear(JohnRingBuffer *ring_buffer) {
    if (ring_buffer) {
        memset(ring_buffer->array, 0, sizeof(void *) * ring_buffer->capacity);
        ring_buffer->read_pointer = 0;
        ring_buffer->write_pointer = 0;
        ring_buffer->round = 0;
    }
}
