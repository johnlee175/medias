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
#ifndef X264_STREAM_H
#define X264_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef void (*OnFrameEncodedFunc)(uint8_t *payload, uint32_t size);

typedef struct X264Stream X264Stream;

/**
 * csp, preset, profile see <x264.h>.
 * csp is macro start with X264_CSP_*, preset and profile see x264_preset_names[], x264_profile_names[].
 * if want default value, set csp = -1(default I420), preset = NULL(default "medium"), profile = NULL(default "high").
 */
X264Stream *create_x264_module(int width, int height,
                               int csp, const char *preset, const char *profile,
                               OnFrameEncodedFunc func);
int append_i420_frame(X264Stream *stream, uint8_t *frame_data);
int encode_x264_frame(X264Stream *stream);
void destroy_x264_module(X264Stream *stream);

#ifdef __cplusplus
}
#endif

#endif /* X264_STREAM_H */
