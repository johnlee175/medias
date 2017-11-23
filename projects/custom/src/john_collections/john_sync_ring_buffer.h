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
#ifndef __JOHN_SYNC_RING_BUFFER_H__
#define __JOHN_SYNC_RING_BUFFER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct JohnSyncRingBuffer JohnSyncRingBuffer;

JohnSyncRingBuffer *john_sync_ring_buffer_create(uint32_t capacity, void *return_on_empty);
void john_sync_ring_buffer_destroy(JohnSyncRingBuffer *ring_buffer);
void *john_sync_ring_buffer_read(JohnSyncRingBuffer *ring_buffer, int32_t timeout_millis);
void *john_sync_ring_buffer_write(JohnSyncRingBuffer *ring_buffer, void *data);
void john_sync_ring_buffer_clear(JohnSyncRingBuffer *ring_buffer);

#ifdef __cplusplus
}
#endif

#endif /* __JOHN_SYNC_RING_BUFFER_H__ */
