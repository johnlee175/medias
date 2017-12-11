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

typedef void (*OnFrameEncodedFunc)(uint8_t *payload, uint32_t size, void *user_data);
/** x264_param_type can be type cast as x264_param_t. see default_param_config(void *) **/
typedef void (*OnParamConfigFunc)(void *x264_param_type);

typedef struct X264Stream X264Stream;

/**
 * #include <x264.h>
 * void default_param_config(void *x264_param_type) {
 *     x264_param_t *param = x264_param_type;
 *     const int framerate = 25;
 *     const int bitrate = 500; // kbps
 *
 *     param->i_level_idc = 30;
 *     param->i_nal_hrd = X264_NAL_HRD_VBR;
 *
 *     param->i_fps_num = (uint32_t) framerate;
 *     param->i_fps_den = 1;
 *     param->i_keyint_max = framerate * 2;
 *     param->i_frame_reference = 10;
 *
 *     param->rc.i_rc_method = X264_RC_ABR;
 *     param->rc.i_bitrate = bitrate;
 *     param->rc.i_vbv_max_bitrate = (int) (bitrate * 1.2);
 *     param->rc.i_vbv_buffer_size = param->rc.i_bitrate;
 *     param->rc.f_vbv_buffer_init = 0.9;
 *     param->rc.b_mb_tree = 0;
 *     param->rc.f_rf_constant = 25;
 *     param->rc.f_rf_constant_max = 45;
 *     param->rc.f_rate_tolerance = 0.01;
 *
 *     param->b_vfr_input = 0;
 *     param->b_repeat_headers = 1;
 *     param->b_annexb = 1;
 *     param->b_aud = 1;
 *     param->b_cabac = 1;
 *
 *     param->i_log_level = X264_LOG_DEBUG;
 * }
 */
void default_param_config(void *x264_param_type);

/**
 * csp, preset, profile, tune see <x264.h>.
 * csp is macro start with X264_CSP_*.
 * preset, profile, tune see x264_preset_names[], x264_profile_names[], x264_tune_names[].
 * if want default value, set csp = -1(default I420),
 * preset = NULL(default "medium"), profile = NULL(default "high"), tune = NULL(default NULL),
 * config_func = NULL(default default_param_config)
 */
X264Stream *create_x264_module(int width, int height, int csp,
                               const char *preset, const char *profile, const char *tune,
                               OnParamConfigFunc config_func, OnFrameEncodedFunc encoded_func,
                               void *user_data);
int append_i420_frame(X264Stream *stream, uint8_t *frame_data);
int encode_x264_frame(X264Stream *stream);
void destroy_x264_module(X264Stream *stream);

#ifdef __cplusplus
}
#endif

#endif /* X264_STREAM_H */
