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
 * @version 2017-12-27
 */
#ifndef FFMPEG_BASE_CLIENT_H
#define FFMPEG_BASE_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>

typedef void (*VideoFrameCallback)(uint8_t *data[8], int line_size[8], uint32_t pixel_precision,
                                   uint32_t width, uint32_t height, int64_t pts_millis,
                                   void *user_tag);
typedef void (*AudioFrameCallback)(uint8_t *data[8], int line_size[8], uint32_t sample_precision,
                                   uint32_t nb_channels, uint32_t sample_rate, int64_t pts_millis,
                                   void *user_tag);

typedef struct VideoOutRule {
    VideoFrameCallback callback;
    void *user_tag;
    int width;
    int height;
    enum AVPixelFormat pixel_format;
} VideoOutRule;

typedef struct AudioOutRule {
    AudioFrameCallback callback;
    void *user_tag;
    uint64_t channel_layout;
    int sample_rate;
    enum AVSampleFormat sample_format;
} AudioOutRule;

typedef struct FFmpegClient FFmpegClient;

FFmpegClient *open_media(const char *url, VideoOutRule *video_out, AudioOutRule *audio_out);
void loop_read_frame(FFmpegClient *client);
void close_media(FFmpegClient *client);

#ifdef __cplusplus
}
#endif

#endif /* FFMPEG_BASE_CLIENT_H */
