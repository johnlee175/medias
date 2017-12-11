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
#include "common.h"
#include "x264_stream.h"
#include <x264.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

struct X264Stream {
    int width;
    int height;
    int format;
    x264_picture_t *pic_in;
    x264_picture_t *pic_out;
    x264_t *h;
    x264_nal_t *nal;
    int i_nal;
    OnFrameEncodedFunc func;
};

void default_param_config(void *x264_param_type) {
    x264_param_t *param = x264_param_type;
    const int framerate = 25;
    const int bitrate = 500; /* kbps */

    param->i_level_idc = 30;
    param->i_nal_hrd = X264_NAL_HRD_VBR;

    param->i_fps_num = (uint32_t) framerate;
    param->i_fps_den = 1;
    param->i_keyint_max = framerate * 2;
    param->i_frame_reference = 10;

    param->rc.i_rc_method = X264_RC_ABR;
    param->rc.i_bitrate = bitrate;
    param->rc.i_vbv_max_bitrate = (int) (bitrate * 1.2);
    param->rc.i_vbv_buffer_size = param->rc.i_bitrate;
    param->rc.f_vbv_buffer_init = 0.9;
    param->rc.b_mb_tree = 0;
    param->rc.f_rf_constant = 25;
    param->rc.f_rf_constant_max = 45;
    param->rc.f_rate_tolerance = 0.01;

    param->b_vfr_input = 0;
    param->b_repeat_headers = 1;
    param->b_annexb = 1;
    param->b_aud = 1;
    param->b_cabac = 1;

    param->i_log_level = X264_LOG_NONE;
}

X264Stream *create_x264_module(int width, int height, int csp,
                               const char *preset, const char *profile, const char *tune,
                               OnParamConfigFunc config_func, OnFrameEncodedFunc encoded_func) {
    X264Stream *stream = (X264Stream *) malloc(sizeof(X264Stream));
    if (!stream) {
        LOGW("malloc X264Stream failed!\n");
        return NULL;
    }
    memset(stream, 0, sizeof(X264Stream));
    stream->func = encoded_func;

    x264_param_t param;
    /* Get default params for preset/tuning */
    if (x264_param_default_preset(&param, (preset ? preset : "medium"),
                                  (tune ? tune : NULL)) < 0) {
        LOGW("x264_param_default_preset failed!\n");
        return NULL;
    }

    /* Configure non-default params */
    param.i_csp = stream->format = (csp > 0 ? csp : X264_CSP_I420);
    param.i_width = stream->width = width;
    param.i_height = stream->height = height;
    if (config_func) {
        config_func(&param);
    } else {
        default_param_config(&param);
    }

    /* Apply profile restrictions. */
    if (x264_param_apply_profile(&param, (profile ? profile : "high")) < 0) {
        LOGW("x264_param_apply_profile failed!\n");
        return NULL;
    }

    stream->pic_in = (x264_picture_t *) malloc(sizeof(x264_picture_t));
    if (!stream->pic_in) {
        LOGW("malloc x264_picture_t[pic_in] failed!\n");
        return NULL;
    }
    if (x264_picture_alloc(stream->pic_in, param.i_csp, param.i_width, param.i_height) < 0) {
        LOGW("x264_picture_alloc failed!\n");
        return NULL;
    }

    stream->pic_out = (x264_picture_t *) malloc(sizeof(x264_picture_t));
    if (!stream->pic_out) {
        LOGW("malloc x264_picture_t[pic_out] failed!\n");
        return NULL;
    }

    stream->h = x264_encoder_open(&param);
    if (!stream->h) {
        LOGW("x264_encoder_open failed!\n");
        x264_picture_clean(stream->pic_in);
        return NULL;
    }

    return stream;
}

int append_i420_frame(X264Stream *stream, uint8_t *frame_data) {
    int luma_size = stream->width * stream->height;
    if (stream->format == X264_CSP_I420 || stream->format == X264_CSP_YV12) {
        int chroma_size = luma_size / 4;
        memcpy(stream->pic_in->img.plane[0], frame_data, luma_size);
        memcpy(stream->pic_in->img.plane[1], frame_data + luma_size, chroma_size);
        memcpy(stream->pic_in->img.plane[2], frame_data + luma_size + chroma_size, chroma_size);
    } else if (stream->format == X264_CSP_NV12 || stream->format == X264_CSP_NV21) {
        memcpy(stream->pic_in->img.plane[0], frame_data, luma_size);
        memcpy(stream->pic_in->img.plane[1], frame_data + luma_size, luma_size);
    } else {
        LOGW("Found not support format yet!\n");
        return -1;
    }

    ++stream->pic_in->i_pts;

    int i_frame_size;
    i_frame_size = x264_encoder_encode(stream->h, &stream->nal, &stream->i_nal, stream->pic_in, stream->pic_out);
    if (i_frame_size < 0) {
        return -1;
    } else if (i_frame_size) {
        stream->func(stream->nal->p_payload, (uint32_t) i_frame_size);
    }

    return 0;
}

int encode_x264_frame(X264Stream *stream) {
    int i_frame_size;
    while (x264_encoder_delayed_frames(stream->h)) {
        i_frame_size = x264_encoder_encode(stream->h, &stream->nal, &stream->i_nal, NULL, stream->pic_out);
        if (i_frame_size < 0) {
            return -1;
        } else if (i_frame_size) {
            stream->func(stream->nal->p_payload, (uint32_t) i_frame_size);
        }
    }
    return 0;
}

void destroy_x264_module(X264Stream *stream) {
    if (stream) {
        if (stream->h) {
            x264_encoder_close(stream->h);
            stream->h = NULL;
        }
        if (stream->pic_in) {
            x264_picture_clean(stream->pic_in);
            free(stream->pic_in);
            stream->pic_in = NULL;
        }
        if (stream->pic_out) {
            free(stream->pic_out);
            stream->pic_out = NULL;
        }
        free(stream);
    }
}

/* ====================== testing ====================== */

static void on_frame_encoded(uint8_t *payload, uint32_t size) {
    FILE *out = fopen("data/test.264", "a");
    if (!fwrite(payload, size, 1, out)) {
        LOGW("on_frame_encoded process failed!\n");
    }
    fclose(out);
}

static int test_x264() {
    const int width = 320, height = 180;
    // ffmpeg -i data/cuc_ieschool.mp4 -c:v rawvideo -pix_fmt yuv420p data/cuc_ieschool.yuv
    const char *yuv_file = "data/cuc_ieschool.yuv";
    X264Stream *stream = NULL;
    if (!(stream = create_x264_module(width, height, -1, NULL, NULL, "zerolatency",
                                      NULL, on_frame_encoded))) {
        LOGW("create_x264_module failed!\n");
        return -1;
    }

    size_t frame_size = width * height * 3 / 2;
    uint8_t *frame = (uint8_t *) malloc(frame_size);

    FILE *file = fopen(yuv_file, "r");
    while (fread(frame, 1, frame_size, file) == frame_size) {
        if (append_i420_frame(stream, frame) < 0) {
            LOGW("append_i420_frame failed!\n");
            goto fail;
        }
    }
    if (encode_x264_frame(stream) < 0) {
        LOGW("encode_x264_frame failed!\n");
        goto fail;
    }

    free(frame);
    destroy_x264_module(stream);
    return 0;

    fail:
    free(frame);
    destroy_x264_module(stream);
    return -1;
}
