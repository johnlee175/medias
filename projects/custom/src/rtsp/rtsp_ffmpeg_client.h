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
#ifndef RTSP_FFMPEG_CLIENT_H
#define RTSP_FFMPEG_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <libavutil/pixfmt.h>

typedef void (*FrameCallback)(uint8_t *data[8], int line_size[8],
                              uint32_t width, uint32_t height, int64_t pts_millis);

typedef struct RtspClient RtspClient;

RtspClient * open_rtsp(const char *rtsp_url, enum AVPixelFormat pixel_format, FrameCallback frame_callback);
void loop_read_rtsp_frame(RtspClient *client);
void close_rtsp(RtspClient *client);

#ifdef __cplusplus
}
#endif

#endif /* RTSP_FFMPEG_CLIENT_H */
