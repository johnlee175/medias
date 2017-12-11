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
#include <stdlib.h>
#include <memory.h>
#include <sys/time.h>
#include <librtmp/rtmp.h>
#include "john_synchronized_queue.h"
#include "common.h"
#include "rtmp_publisher.h"
#include "sps_decode.h"
#include "trace_malloc_free.h"

struct RtmpPublisher {
    RTMP *rtmp;
    RtmpSourceFunc func;
    JohnSynchronizedQueue *unit_queue;
    JohnSynchronizedQueue *buffer_queue;
    uint32_t unit_queue_length;
    uint32_t buffer_queue_length;
};

typedef struct RtmpByteArray {
    uint32_t size;
    uint8_t *data;
} RtmpByteArray;

#define put_amf_string(str, len, body_pointer) \
                *(body_pointer)++ = AMF_STRING; \
                (len) = (uint16_t) strlen((str)); \
                *(body_pointer)++ = (uint8_t)(((len) >> 8) & 0xFF); \
                *(body_pointer)++ = (uint8_t)(((len) >> 0) & 0xFF); \
                memcpy((body_pointer), (str), (len)); \
                (body_pointer) += (len);

#define put_amf_number(d, body_pointer) \
                *(body_pointer)++ = AMF_NUMBER; \
                for (int i = sizeof((d)) - 1; i >= 0; --i) { \
                    *(body_pointer)++ = (uint8_t) (((d) >> (i * 8)) & 0xFF); \
                }

#define is_nalu_type(nalu, type) (((nalu)->data[0] & 0x1F) == (type))

static uint32_t rtmp_publisher_read_source(RtmpPublisher *publisher, uint8_t **buffer, const bool *quit) {
    if (publisher && publisher->buffer_queue && buffer) {
        void *data = NULL;
        while(!*quit) {
            data = john_synchronized_queue_dequeue(publisher->buffer_queue, 1000);
            if (data) {
                LOGW("buffer queue length == %ul\n", --publisher->buffer_queue_length);
                RtmpByteArray *array = (RtmpByteArray *) data;
                *buffer = array->data;
                uint32_t size = array->size;
                free(array);
                return size;
            }
        }
    }
    return 0;
}

bool rtmp_publisher_update_source(RtmpPublisher *publisher,
                                  uint8_t *buffer/* 500 in queue */, uint32_t buffer_size) {
    if (publisher && publisher->buffer_queue && buffer && buffer_size > 0) {
        RtmpByteArray *oldest = NULL;
        RtmpByteArray *newer = (RtmpByteArray *) malloc(sizeof(RtmpByteArray));
        if (!newer) {
            LOGW("malloc RtmpByteArray *newer in rtmp_publisher_update_source failed!\n");
            return false;
        }
        newer->size = buffer_size;
        newer->data = (uint8_t *) malloc(buffer_size);
        if (!newer->data) {
            LOGW("malloc uint8_t *newer->data in rtmp_publisher_update_source failed!\n");
            return false;
        }
        memcpy(newer->data, buffer, buffer_size);
        if (john_synchronized_queue_enqueue(publisher->buffer_queue, newer,
                                            (void **) &oldest, -1)) {
            LOGW("buffer queue length == %ul\n", ++publisher->buffer_queue_length);
            if (oldest) {
                if (oldest->data) {
                    free(oldest->data);
                    oldest->data = NULL;
                }
                free(oldest);
            }
            return true;
        } else {
            LOGW("john_synchronized_queue_enqueue failed!\n");
        }
    }
    return false;
}

RtmpPublisher *rtmp_publisher_create(char *rtmp_url, RtmpSourceFunc func) {
    RtmpPublisher *publisher = (RtmpPublisher *) malloc(sizeof(RtmpPublisher));
    if (!publisher) {
        LOGW("malloc RtmpPublisher *publisher in rtmp_publisher_create failed!\n");
        return NULL;
    }
    memset(publisher, 0, sizeof(RtmpPublisher));

    if (func) { /* [pull] using func to get source data */
        publisher->func = func;
    } else { /* [push] user should be call rtmp_publisher_update_source(...) for add data to queue */
        publisher->func = rtmp_publisher_read_source;
        publisher->buffer_queue = john_synchronized_queue_create(500, true, NULL);
        if (!publisher->buffer_queue) {
            LOGW("john_synchronized_queue_create failed!\n");
            free(publisher);
            return NULL;
        }
    }

    publisher->unit_queue = john_synchronized_queue_create(500, true, NULL);
    if (!publisher->unit_queue) {
        LOGW("john_synchronized_queue_create failed!\n");
        free(publisher);
        return NULL;
    }

    publisher->rtmp = RTMP_Alloc();
    if (!publisher->rtmp) {
        LOGW("RTMP_Alloc failed!\n");
        free(publisher);
        return NULL;
    }

    RTMP_Init(publisher->rtmp);
    publisher->rtmp->Link.timeout = 15;
    if (!RTMP_SetupURL(publisher->rtmp, rtmp_url)) {
        LOGW("RTMP_SetupURL failed!\n");
        goto error_after_alloc;
    }
    RTMP_EnableWrite(publisher->rtmp);
    if (!RTMP_Connect(publisher->rtmp, NULL)) {
        LOGW("RTMP_Connect failed!\n");
        goto error_after_alloc;
    }
    if (!RTMP_ConnectStream(publisher->rtmp, 0)) {
        LOGW("RTMP_ConnectStream failed!\n");
        RTMP_Close(publisher->rtmp);
        goto error_after_alloc;
    }
    return publisher;

    error_after_alloc:
    RTMP_Free(publisher->rtmp);
    free(publisher);
    return NULL;
}

void rtmp_publisher_destroy(RtmpPublisher *publisher) {
    if (publisher) {
        if (publisher->rtmp) {
            RTMP_DeleteStream(publisher->rtmp);
            RTMP_Close(publisher->rtmp);
            RTMP_Free(publisher->rtmp);
            publisher->rtmp = NULL;
            LOGW("rtmp destroy ok!\n");
        }
        if (publisher->unit_queue) {
            john_synchronized_queue_destroy(publisher->unit_queue);
            publisher->unit_queue = NULL;
        }
        if (publisher->buffer_queue) {
            john_synchronized_queue_destroy(publisher->buffer_queue);
            publisher->buffer_queue = NULL;
        }
        free(publisher);
    }
}

bool rtmp_publisher_send(RtmpPublisher *publisher, uint8_t *buffer, uint32_t buffer_size,
        uint8_t packet_type, uint32_t time_stamp) {
    if (!publisher || !publisher->rtmp || !RTMP_IsConnected(publisher->rtmp)) {
        LOGW("publisher == NULL or publisher->rtmp == NULL or RTMP_IsConnected == false!\n");
        return false;
    }
    RTMPPacket *rtmp_packet = (RTMPPacket *) malloc(sizeof(RTMPPacket));
    if (!rtmp_packet) {
        LOGW("malloc RTMPPacket *rtmp_packet in rtmp_publisher_send failed!\n");
        return false;
    }
    memset(rtmp_packet, 0, sizeof(RTMPPacket));
    if (!RTMPPacket_Alloc(rtmp_packet, buffer_size)) {
        LOGW("RTMPPacket_Alloc failed!\n");
        free(rtmp_packet);
        return false;
    }
    RTMPPacket_Reset(rtmp_packet);
    rtmp_packet->m_packetType = packet_type;
    rtmp_packet->m_nBodySize = buffer_size;
    rtmp_packet->m_nTimeStamp = time_stamp;
    rtmp_packet->m_nChannel = 0x04; /* source channel */
    rtmp_packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
    rtmp_packet->m_nInfoField2 = publisher->rtmp->m_stream_id;
    memcpy(rtmp_packet->m_body, buffer, buffer_size);
    bool result = true;
    if (!RTMP_SendPacket(publisher->rtmp, rtmp_packet, 0)) {
        LOGW("RTMP_SendPacket failed!\n");
        result = false;
    }
    RTMPPacket_Free(rtmp_packet);
    free(rtmp_packet);
    return result;
}

bool rtmp_publisher_send_metadata(RtmpPublisher *publisher, uint32_t width, uint32_t height,
                                  uint32_t frame_rate) {
    if (!publisher) {
        return false;
    }

#define rps_body_size 1024
    uint8_t *body = (uint8_t *) malloc(rps_body_size);
    if (!body) {
        LOGW("malloc uint8_t *body in rtmp_publisher_send_metadata failed!\n");
        return false;
    }
    memset(body, 0, rps_body_size);
#undef rps_body_size
    uint8_t *body_pointer = body;
    uint16_t len;

    put_amf_string("width", len, body_pointer);
    put_amf_number((uint64_t) width, body_pointer);
    put_amf_string("height", len, body_pointer);
    put_amf_number((uint64_t) height, body_pointer);
    put_amf_string("framerate", len, body_pointer);
    put_amf_number((uint64_t) frame_rate, body_pointer);
    put_amf_string("videocodecid", len, body_pointer);
    put_amf_number((uint64_t) 7 /* h264 */, body_pointer);
    *(body_pointer)++ = AMF_OBJECT_END;

    bool ret = rtmp_publisher_send(publisher, body, (uint32_t) (body_pointer - body),
                                   RTMP_PACKET_TYPE_INFO, 0);
    free(body);
    return ret;
}

bool rtmp_publisher_send_avc_sequence_header(RtmpPublisher *publisher, uint8_t *sps, uint32_t sps_len,
                                             uint8_t *pps, uint32_t pps_len) {
    if (!publisher || !sps || !pps) {
        return false;
    }

#define rps_body_size 1024
    uint8_t *body = (uint8_t *) malloc(rps_body_size);
    if (!body) {
        LOGW("malloc uint8_t *body in rtmp_publisher_send_avc_sequence_header failed!\n");
        return false;
    }
    memset(body, 0, rps_body_size);
#undef rps_body_size
    uint8_t *body_pointer = body;

    *body_pointer++ = 0x17; /* 1:keyframe, 7:AVC */
    *body_pointer++ = 0x00; /* AVC sequence header */
    /* fill in 0 */
    *body_pointer++ = 0x00;
    *body_pointer++ = 0x00;
    *body_pointer++ = 0x00;
    /* AVCDecoderConfigurationRecord */
    *body_pointer++ = 0x01; /* configurationVersion */
    *body_pointer++ = sps[1]; /* AVCProfileIndication */
    *body_pointer++ = sps[2]; /* profile_compatibility */
    *body_pointer++ = sps[3]; /* AVCLevelIndication */
    *body_pointer++ = 0xFF; /* lengthSizeMinusOne, = 3 */

    /* sps nums, = 1 */
    *body_pointer++ = 0xE1;
    /* sps data length, Big Endian */
    *body_pointer++ = (uint8_t) ((sps_len >> 8) & 0xFF);
    *body_pointer++ = (uint8_t) ((sps_len >> 0) & 0xFF);
    /* sps data */
    memcpy(body_pointer, sps, sps_len);
    body_pointer += sps_len;

    /* pps nums, = 1 */
    *body_pointer++ = 0x01;
    /* pps data length, Big Endian */
    *body_pointer++ = (uint8_t) ((pps_len >> 8) & 0xFF);
    *body_pointer++ = (uint8_t) ((pps_len >> 0) & 0xFF);
    /* pps data */
    memcpy(body_pointer, pps, pps_len);
    body_pointer += pps_len;

    bool ret = rtmp_publisher_send(publisher, body, (uint32_t) (body_pointer - body),
                                   RTMP_PACKET_TYPE_VIDEO, 0);
    free(body);
    return ret;
}

bool rtmp_publisher_send_avc_nalu(RtmpPublisher *publisher, uint8_t *nalu, uint32_t nalu_size,
                                  uint32_t time_stamp, bool is_key_frame) {
    if (!publisher || !nalu) {
        return false;
    }
    const size_t body_size = nalu_size + 9;
    uint8_t *body = (uint8_t *) malloc(body_size);
    if (!body) {
        LOGW("malloc uint8_t *body in rtmp_publisher_send_avc_nalu failed!\n");
        return false;
    }
    memset(body, 0, body_size);
    uint8_t *body_pointer = body;

    if (is_key_frame) {
        *body_pointer++ = 0x17; /* 1:I frame, 7:AVC */
    } else {
        *body_pointer++ = 0x27; /* 2:P frame, 7:AVC */
    }
    *body_pointer++ = 0x01; /* AVC NALU */
    /* fill in 0 */
    *body_pointer++ = 0x00;
    *body_pointer++ = 0x00;
    *body_pointer++ = 0x00;
    /* NALU size, Big Endian */
    *body_pointer++ = (uint8_t) ((nalu_size >> 24) & 0xFF);
    *body_pointer++ = (uint8_t) ((nalu_size >> 16) & 0xFF);
    *body_pointer++ = (uint8_t) ((nalu_size >> 8) & 0xFF);
    *body_pointer++ = (uint8_t) ((nalu_size >> 0) & 0xFF);
    /* NALU data */
    memcpy(body_pointer, nalu, nalu_size);
    body_pointer += nalu_size;
    bool ret = rtmp_publisher_send(publisher, body, (uint32_t) (body_pointer - body),
                                   RTMP_PACKET_TYPE_VIDEO, time_stamp);
    free(body);
    return ret;
}

bool rtmp_publisher_enqueue_avc_nalu(RtmpPublisher *publisher, uint8_t *buffer,
                                     uint32_t nal_start, uint32_t nal_end) {
    RtmpByteArray *nalu = (RtmpByteArray *) malloc(sizeof(RtmpByteArray));
    if (!nalu) {
        LOGW("malloc RtmpByteArray *nalu in rtmp_publisher_enqueue_avc_nalu failed!\n");
        return false;
    }
    memset(nalu, 0, sizeof(RtmpByteArray));
    nalu->size = (nal_end - nal_start) + 1;
    nalu->data = (uint8_t *) malloc(nalu->size);
    if (!nalu->data) {
        LOGW("malloc uint8_t *nalu->data in rtmp_publisher_enqueue_avc_nalu failed!\n");
        return false;
    }
    memcpy(nalu->data, buffer + nal_start, nalu->size);
    RtmpByteArray *oldest = NULL;
    if (!john_synchronized_queue_enqueue(publisher->unit_queue, nalu, (void **) &oldest, -1)) {
        LOGW("john_synchronized_queue_enqueue failed!\n");
        return false;
    }
    LOGW("unit queue length == %ul\n", ++publisher->unit_queue_length);
    LOGW("avc nalu enqueue!\n");
    if (oldest) {
        if (oldest->data) {
            free(oldest->data);
        }
        free(oldest);
    }
    return true;
}

bool rtmp_publisher_avc_produce_loop(RtmpPublisher *publisher, const bool *quit) {
    if (!publisher || !publisher->func) {
        return false;
    }
    const bool default_quit = false;
    if (!quit) {
        quit = &default_quit;
    }
    uint8_t *buffer = NULL, *remaining = NULL, *temp = NULL, *older = NULL;
    uint32_t read_count = 0, read_position = 0, remaining_len = 0;
    uint32_t nal_start = 0, nal_end = 0;
    bool started = false;
    while ((read_count = publisher->func(publisher, &buffer, quit)) > 0) {
        if (*quit) {
            goto end;
        }
        if (remaining && remaining_len > 0) {
            temp = (uint8_t *) malloc(remaining_len + read_count);
            if (!temp) {
                LOGW("malloc uint8_t *temp in rtmp_publisher_avc_produce_loop failed!\n");
                return false;
            }
            memcpy(temp, remaining, remaining_len);
            memcpy(temp + remaining_len, buffer, read_count);
            free(buffer);
            buffer = temp;
            read_count += remaining_len;
            read_position = 0;
            if (older) {
                free(older);
                older = NULL;
            }
        }
        while ((read_position + 3) < read_count) {
            if (*quit) {
                goto end;
            }
            if (buffer[read_position] == 0x00
                && buffer[read_position + 1] == 0x00
                && buffer[read_position + 2] == 0x01) {
                if (!started) {
                    nal_start = read_position + 3;
                    started = true;
                    read_position += 4;
                } else {
                    if (buffer[read_position - 1] == 0x00) {
                        --read_position;
                    }
                    nal_end = read_position - 1;
                    started = false;
                    if (!rtmp_publisher_enqueue_avc_nalu(publisher, buffer, nal_start, nal_end)) {
                        LOGW("rtmp_publisher_enqueue_nalu failed!\n");
                        return false;
                    }
                }
            } else {
                ++read_position;
            }
        }
        if (started) {
            remaining = buffer + nal_start;
            remaining_len = read_count - nal_start;
            nal_start = 0;
        } else {
            remaining = buffer + read_position;
            remaining_len = read_count - read_position;
        }
        if (remaining_len <= 0) {
            free(buffer);
        } else {
            older = buffer;
        }
        LOGW("produce round successful!\n");
    }
    LOGW("produce while done\n");
    end:
    return true;
}

#ifndef QUIT_LOOP
#define QUIT_LOOP abort()
#endif /* QUIT_LOOP */

bool rtmp_publisher_avc_consume_loop(RtmpPublisher *publisher, const bool *quit) {
    bool default_quit = false;
    if (!quit) {
        quit = &default_quit;
    }
    RtmpByteArray *nalu = NULL;
    uint8_t *sps = NULL, *pps = NULL;
    uint32_t sps_len = 0, pps_len = 0;
    uint32_t width = 0, height = 0, frame_rate = 0;
    bool send_meta_header = false;
    time_t pts_millis;
    struct timeval start_time, end_time;
    if (gettimeofday(&start_time, NULL)) {
        LOGW("call gettimeofday failed\n");
        return false;
    }
    while (!*quit) {
        nalu = (RtmpByteArray *)john_synchronized_queue_dequeue(publisher->unit_queue, 1000);
        if (nalu) {
            LOGW("unit queue length == %ul\n", --publisher->unit_queue_length);
            if (is_nalu_type(nalu, 0x07)) {
                sps = nalu->data;
                sps_len = nalu->size;
                if (!decode_h264_sps(nalu->data, nalu->size, &width, &height, &frame_rate)) {
                    LOGW("decode_h264_sps failed\n");
                    QUIT_LOOP;
                } else {
                    LOGW("decode_h264_sps: width=%d, height=%d, frame rate=%d\n", width, height, frame_rate);
                }
            } else if (is_nalu_type(nalu, 0x08)) {
                pps = nalu->data;
                pps_len = nalu->size;
            } else if (is_nalu_type(nalu, 0x0b)) {
                LOGW("detected end of stream!\n");
                break;
            } else {
                if (!send_meta_header) {
                    if (sps && pps) {
                        send_meta_header = true;
                        if (!rtmp_publisher_send_metadata(publisher, width, height, frame_rate)) {
                            LOGW("metadata send failed\n");
                            QUIT_LOOP;
                        }
                        if (!rtmp_publisher_send_avc_sequence_header(publisher, sps, sps_len, pps, pps_len)) {
                            LOGW("avc sequence header send failed\n");
                            QUIT_LOOP;
                        }
                        free(sps);
                        free(pps);
                        LOGW("metadata and avc sequence header send successful!\n");
                    }
                } else {
                    if (gettimeofday(&end_time, NULL)) {
                        LOGW("call gettimeofday failed\n");
                    }
                    pts_millis = 1000 * (end_time.tv_sec - start_time.tv_sec) +
                            ((end_time.tv_usec - start_time.tv_usec) / 1000);
                    if (!rtmp_publisher_send_avc_nalu(publisher, nalu->data, nalu->size,
                                                 (uint32_t) pts_millis, is_nalu_type(nalu, 0x05 /* IDR */))) {
                        LOGW("avc nalu send failed\n");
                        QUIT_LOOP;
                    }
                    LOGW("consume round successful!\n");
                }
                if (nalu->data) {
                    free(nalu->data);
                    nalu->data = NULL;
                }
            }
            free(nalu);
        }
    }
    return true;
}
