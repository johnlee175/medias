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
 * @version 2017-12-10
 */
#ifndef X265_STREAM_H
#define X265_STREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef void (*OnX265FrameEncodedFunc)(uint8_t *payload, uint32_t size, void *user_data);
/** x265_param_type can be type cast as x265_param pointer. see default_param_config_x265(void *) **/
typedef void (*OnX265ParamConfigFunc)(void *x265_param_type);

typedef struct X265Stream X265Stream;

/**
 * #include <x265.h>
 * void default_param_config_x265(void *x265_param_type) {
 *     x265_param *param = (x265_param *) x265_param_type;
 *     const int framerate = 25;
 *     const int bitrate = 500; // kbps
 *
 *     param->levelIdc = 30;
 *     param->fpsNum = (uint32_t) framerate;
 *     param->fpsDenom = 1;
 *     param->keyframeMax = framerate * 2;
 *     param->keyframeMin = framerate / 2;
 *
 *     param->limitTU = 4;
 *     param->tuQTMaxInterDepth = 2;
 *     param->maxTUSize = 16;
 *
 *     param->rc.rateControlMode = X265_RC_ABR;
 *     param->rc.bitrate = bitrate;
 *     param->rc.vbvMaxBitrate = (int) (bitrate * 1.2);
 *     param->rc.vbvBufferSize = param->rc.bitrate;
 *     param->rc.vbvBufferInit = 0.9;
 *     param->rc.cuTree = 0;
 *     param->rc.rfConstant = 25;
 *     param->rc.rfConstantMax = 45;
 *
 *     param->bRepeatHeaders = 1;
 *     param->bAnnexB = 1;
 *
 *     param->logLevel = X265_LOG_NONE;
 * }
 */
void default_param_config_x265(void *x265_param_type);

/**
 * csp, preset, profile, tune see <x265.h>.
 * csp is macro start with X265_CSP_*,
 * default support X265_CSP_I420, X265_CSP_NV12, X265_CSP_YV12, X265_CSP_NV21.
 * preset, profile, tune see x265_preset_names[], x265_profile_names[], x265_tune_names[].
 * if want default value, set csp = -1(default I420),
 * preset = NULL(default "medium"), profile = NULL(default "main"), tune = NULL(default "fastdecode"),
 * config_func = NULL(default default_param_config_x265)
 */
X265Stream *create_x265_module(int width, int height, int csp,
                               const char *preset, const char *profile, const char *tune,
                               OnX265ParamConfigFunc config_func, OnX265FrameEncodedFunc encoded_func,
                               void *user_data);
int append_yuv_frame_x265(X265Stream *stream, uint8_t *frame_data);
int encode_x265_frame(X265Stream *stream);
void destroy_x265_module(X265Stream *stream);

#define X265_CSP_YV12 0x1000
#define X265_CSP_NV21 0x2000

#ifdef __cplusplus
}
#endif

#endif /* X265_STREAM_H */
