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
#ifndef __JOHN_BLOCKING_QUEUE_H__
#define __JOHN_BLOCKING_QUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct JohnSynchronizedQueue JohnSynchronizedQueue;

JohnSynchronizedQueue *john_synchronized_queue_create(uint32_t capacity,
                                              bool replace_oldest, void *return_on_empty);
void john_synchronized_queue_destroy(JohnSynchronizedQueue *synchronized_queue);
bool john_synchronized_queue_enqueue(JohnSynchronizedQueue *synchronized_queue, void *data,
                                      void **oldest, int32_t timeout_millis);
void *john_synchronized_queue_dequeue(JohnSynchronizedQueue *synchronized_queue, int32_t timeout_millis);
void *john_synchronized_queue_head(JohnSynchronizedQueue *synchronized_queue);
void *john_synchronized_queue_tail(JohnSynchronizedQueue *synchronized_queue);
bool john_synchronized_queue_is_full(JohnSynchronizedQueue *synchronized_queue);
bool john_synchronized_queue_is_empty(JohnSynchronizedQueue *synchronized_queue);
void john_synchronized_queue_clear(JohnSynchronizedQueue *synchronized_queue);

#ifdef __cplusplus
}
#endif

#endif /* __JOHN_BLOCKING_QUEUE_H__ */
