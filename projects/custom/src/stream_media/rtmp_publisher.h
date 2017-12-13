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
 * @version 2017-11-24
 */
#ifndef RTMP_PUBLISHER_H
#define RTMP_PUBLISHER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct RtmpPublisher RtmpPublisher;

typedef uint32_t (*RtmpSourceFunc)(RtmpPublisher *publisher, uint8_t **buffer, const bool *quit);

#ifndef RTMP_PUBLISHER_QUEUE_SIZE
#define RTMP_PUBLISHER_QUEUE_SIZE 500
#endif

RtmpPublisher *rtmp_publisher_create(char *rtmp_url, RtmpSourceFunc func);
void rtmp_publisher_destroy(RtmpPublisher *publisher);
bool rtmp_publisher_send(RtmpPublisher *publisher, uint8_t *buffer, uint32_t buffer_size,
                         uint8_t packet_type, uint32_t time_stamp);

bool rtmp_publisher_update_source(RtmpPublisher *publisher,
                                  uint8_t *buffer/* 500 in queue */, uint32_t buffer_size);

bool rtmp_publisher_avc_produce_loop(RtmpPublisher *publisher, const bool *quit);
/** protocol_version = { 264, 265 } */
bool rtmp_publisher_avc_consume_loop(RtmpPublisher *publisher, const bool *quit, int protocol_version);

#ifdef __cplusplus
}
#endif

#endif /* RTMP_PUBLISHER_H */
