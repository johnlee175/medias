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
#include "john_queue.h"

typedef struct JohnQueueNode {
    void *data;
    struct JohnQueueNode *next;
} JohnQueueNode;

struct JohnQueue {
    uint32_t capacity;
    uint32_t size;
    JohnQueueNode *head;
    JohnQueueNode *tail;
};

JohnQueue *john_queue_create(uint32_t capacity) {
    JohnQueue *queue = (JohnQueue *) malloc(sizeof(JohnQueue));
    if (queue) {
        memset(queue, 0, sizeof(JohnQueue));
        queue->capacity = capacity;
        queue->size = 0;
        queue->head = NULL;
        queue->tail = NULL;
    }
    return queue;
}

void john_queue_destroy(JohnQueue *queue) {
    if (queue) {
        john_queue_clear(queue);
        memset(queue, 0, sizeof(JohnQueue));
        free(queue);
    }
}

bool john_queue_enqueue(JohnQueue *queue, void *data) {
    if (queue && data && queue->size < queue->capacity) {
        JohnQueueNode *node = (JohnQueueNode *)malloc(sizeof(JohnQueueNode));
        if (node) {
            memset(node, 0, sizeof(JohnQueueNode));
            node->data = data;
            if (queue->tail) {
                queue->tail->next = node;
            } else if (!queue->head) {
                queue->head = node;
            }
            queue->tail = node;
            ++queue->size;
            return true;
        }
    }
    return false;
}

void *john_queue_dequeue(JohnQueue *queue) {
    if (queue && queue->head && queue->size > 0) {
        void *data = queue->head->data;
        --queue->size;
        if (queue->head == queue->tail) {
            free(queue->head);
            queue->head = NULL;
            queue->tail = NULL;
        } else if (queue->head->next) {
            JohnQueueNode *node = queue->head;
            queue->head = node->next;
            node->next = NULL;
            free(node);
        }
        return data;
    }
    return NULL;
}

void *john_queue_head(JohnQueue *queue) {
    if (queue && queue->head) {
        return queue->head->data;
    }
    return NULL;
}

void *john_queue_tail(JohnQueue *queue) {
    if (queue && queue->tail) {
        return queue->tail->data;
    }
    return NULL;
}

bool john_queue_is_full(JohnQueue *queue) {
    return queue && queue->size == queue->capacity;
}

bool john_queue_is_empty(JohnQueue *queue) {
    return queue && queue->size == 0;
}

void john_queue_clear(JohnQueue *queue) {
    if (queue) {
        while (!john_queue_is_empty(queue)) {
            john_queue_dequeue(queue);
        }
    }
}
