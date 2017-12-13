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
#include <stdint.h>
#include <x265.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "common.h"
#include "x265_stream.h"

struct X265Stream {
    int width;
    int height;
    int format;
    x265_picture *pic_in;
    x265_picture *pic_out;
    x265_encoder *h;
    x265_param *param;
    x265_nal *nal;
    uint32_t i_nal;
    OnX265FrameEncodedFunc func;
    void *user_data;
};

void default_param_config_x265(void *x265_param_type) {
    x265_param *param = (x265_param *) x265_param_type;
    const int framerate = 25;
    const int bitrate = 500; /* kbps */

    param->levelIdc = 30;
    param->fpsNum = (uint32_t) framerate;
    param->fpsDenom = 1;
    param->keyframeMax = framerate * 2;
    param->keyframeMin = framerate / 2;

    param->limitTU = 4;
    param->tuQTMaxInterDepth = 2;
    param->maxTUSize = 16;

    param->rc.rateControlMode = X265_RC_ABR;
    param->rc.bitrate = bitrate;
    param->rc.vbvMaxBitrate = (int) (bitrate * 1.2);
    param->rc.vbvBufferSize = param->rc.bitrate;
    param->rc.vbvBufferInit = 0.9;
    param->rc.cuTree = 0;
    param->rc.rfConstant = 25;
    param->rc.rfConstantMax = 45;

    param->bRepeatHeaders = 1;
    param->bAnnexB = 1;

    param->logLevel = X265_LOG_NONE;
}

X265Stream *create_x265_module(int width, int height, int csp,
                               const char *preset, const char *profile, const char *tune,
                               OnX265ParamConfigFunc config_func, OnX265FrameEncodedFunc encoded_func,
                               void *user_data) {
    X265Stream *stream = (X265Stream *) malloc(sizeof(X265Stream));
    if (!stream) {
        LOGW("malloc x265Stream failed!\n");
        return NULL;
    }
    memset(stream, 0, sizeof(X265Stream));
    stream->func = encoded_func;
    stream->user_data = user_data;

    x265_param *param = x265_param_alloc();
    if (!param) {
        LOGW("malloc x265Stream failed!\n");
        return NULL;
    }

    /* Get default params for preset/tuning */
    /* http://x265.readthedocs.io/en/default/presets.html */
    if (x265_param_default_preset(param, (preset ? preset : "medium"),
                                  (tune ? tune : "fastdecode")) < 0) {
        LOGW("x265_param_default_preset failed!\n");
        return NULL;
    }

    /* Configure non-default params */
    stream->format = (csp > 0 ? csp : X265_CSP_I420);
    param->internalCsp = (csp > 0 && csp < X265_CSP_MAX ? csp : X265_CSP_I420);
    param->sourceWidth = stream->width = width;
    param->sourceHeight = stream->height = height;
    if (config_func) {
        config_func(param);
    } else {
        default_param_config_x265(param);
    }

    /* Apply profile restrictions. */
    if (x265_param_apply_profile(param, (profile ? profile : "main")) < 0) {
        LOGW("x265_param_apply_profile failed!\n");
        return NULL;
    }

    stream->pic_in = x265_picture_alloc();
    if (!stream->pic_in) {
        LOGW("malloc x265_picture_alloc[pic_in] failed!\n");
        return NULL;
    }
    x265_picture_init(param, stream->pic_in);

    stream->pic_out = x265_picture_alloc();
    if (!stream->pic_out) {
        LOGW("malloc x265_picture_alloc[pic_out] failed!\n");
        return NULL;
    }
    x265_picture_init(param, stream->pic_out);

    stream->h = x265_encoder_open(param);
    if (!stream->h) {
        LOGW("x265_encoder_open failed!\n");
        return NULL;
    }

    stream->param = param;
    return stream;
}

int append_yuv_frame_x265(X265Stream *stream, uint8_t *frame_data) {
    int luma_size = stream->width * stream->height;
    if (stream->format == X265_CSP_I420 || stream->format == X265_CSP_YV12) {
        int chroma_size = luma_size / 4;
        stream->pic_in->stride[0] = stream->width;
        stream->pic_in->stride[1] = stream->width / 2;
        stream->pic_in->stride[2] = stream->width / 2;
        stream->pic_in->planes[0] = frame_data;
        stream->pic_in->planes[1] = frame_data + luma_size;
        stream->pic_in->planes[2] = frame_data + luma_size + chroma_size;
    } else if (stream->format == X265_CSP_NV12 || stream->format == X265_CSP_NV21) {
        stream->pic_in->stride[0] = luma_size;
        stream->pic_in->stride[1] = luma_size;
        stream->pic_in->planes[0] = frame_data;
        stream->pic_in->planes[1] = frame_data + luma_size;
    } else {
        LOGW("Found not support format yet!\n");
        return -1;
    }

    ++stream->pic_in->pts;

    int ret = x265_encoder_encode(stream->h, &stream->nal, &stream->i_nal, stream->pic_in, stream->pic_out);
    if (ret > 0) {
        for (uint32_t i = 0; i < stream->i_nal; ++i) {
            stream->func(stream->nal[i].payload, (uint32_t) stream->nal[i].sizeBytes, stream->user_data);
        }
    }

    return ret;
}

int encode_x265_frame(X265Stream *stream) {
    while (x265_encoder_encode(stream->h, &stream->nal, &stream->i_nal, NULL, stream->pic_out) > 0) {
        for (uint32_t i = 0; i < stream->i_nal; ++i) {
            stream->func(stream->nal[i].payload, stream->nal[i].sizeBytes, stream->user_data);
        }
    }
    return 0;
}

void destroy_x265_module(X265Stream *stream) {
    if (stream) {
        x265_cleanup();
        if (stream->h) {
            x265_encoder_close(stream->h);
            stream->h = NULL;
        }
        if (stream->pic_in) {
            x265_picture_free(stream->pic_in);
            stream->pic_in = NULL;
        }
        if (stream->pic_out) {
            x265_picture_free(stream->pic_out);
            stream->pic_out = NULL;
        }
        if (stream->param) {
            x265_param_free(stream->param);
            stream->param = NULL;
        }
        free(stream);
    }
}

/* ====================== testing ====================== */

#include "base_path.h"

static void on_frame_encoded(uint8_t *payload, uint32_t size, void *user_data) {
    FILE *out = fopen(BASE_PATH"/data/test.265", "a");
    if (!fwrite(payload, size, 1, out)) {
        LOGW("on_frame_encoded process failed!\n");
    }
    fclose(out);
}

static int test_x265() {
    const int width = 512, height = 288;
    // ffmpeg -i data/test.mp4 -c:v rawvideo -pix_fmt yuv420p data/test_512x288.yuv
    const char *yuv_file = BASE_PATH"/data/test_512x288.yuv";
    X265Stream *stream = NULL;
    if (!(stream = create_x265_module(width, height, -1, NULL, NULL, NULL /* "zerolatency" */,
                                      NULL, on_frame_encoded, NULL))) {
        LOGW("create_x265_module failed!\n");
        return -1;
    }

    size_t frame_size = width * height * 3 / 2;
    uint8_t *frame = (uint8_t *) malloc(frame_size);

    FILE *file = fopen(yuv_file, "r");
    while (fread(frame, 1, frame_size, file) == frame_size) {
        if (append_yuv_frame_x265(stream, frame) < 0) {
            LOGW("append_yuv_frame failed!\n");
            goto fail;
        }
    }
    if (encode_x265_frame(stream) < 0) {
        LOGW("encode_x265_frame failed!\n");
        goto fail;
    }

    free(frame);
    destroy_x265_module(stream);
    LOGW("destroy_x265_module normal\n");
    return 0;

    fail:
    free(frame);
    destroy_x265_module(stream);
    LOGW("destroy_x265_module exception\n");
    return -1;
}

#ifdef TEST_X265
int main() {
    test_x265();
}
#endif /* TEST_X265 */