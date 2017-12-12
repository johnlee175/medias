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
 * @version 2017-12-11
 */
#include "common.h"
#include <stdint.h>
#include <x264.h>
#include <x264_stream.h>
#include <rtmp_publisher.h>
#include <stdlib.h>
#include <pthread.h>

static int kfWidth, kfHeight;
static X264Stream *stream = NULL;

#define FRAME_RATE 12

static void on_param_config(void *x264_param_type) {
    x264_param_t *param = (x264_param_t *) x264_param_type;
    param->i_fps_num = (uint32_t) FRAME_RATE;
    param->i_fps_den = 1;
    param->i_keyint_max = FRAME_RATE / 2; /* small for small play delay */
    param->i_frame_reference = 4;
}

static void on_encoded_frame(uint8_t *payload, uint32_t size, void *client) {
    if (!rtmp_publisher_update_source((RtmpPublisher *) client, payload, size)) {
        LOGW("rtmp_publisher_update_source failed!\n");
    }
}

static void do_x264_encode(uint8_t *frame) {
    append_yuv_frame(stream, frame);
}

static void *rtmp_produce(void *client) {
    LOGW("rtmp_produce\n");
    rtmp_publisher_avc_produce_loop((RtmpPublisher *) client, NULL);
    return NULL;
}

static void video_publisher_start() {
    RtmpPublisher *publisher = rtmp_publisher_create("rtmp://192.168.0.102/live", NULL);
    if (!publisher) {
        LOGW("rtmp_publisher_create failed!\n");
        abort();
    }

    if (!(stream = create_x264_module(kfWidth, kfHeight, X264_CSP_YV12,
                                      "fast", "baseline", NULL,
                                      on_param_config, on_encoded_frame, publisher))) {
        LOGW("create_x264_module failed!\n");
        abort();
    }

    pthread_t produce_thread;
    if (pthread_create(&produce_thread, NULL, rtmp_produce, publisher)) {
        LOGW("pthread_create failed!\n");
        abort();
    }

    LOGW("rtmp_consume\n");
    rtmp_publisher_avc_consume_loop(publisher, NULL);

    LOGW("clean and exit...\n");
    destroy_x264_module(stream);
    rtmp_publisher_destroy(publisher);
}

#include <jni.h>

#define JNI_CLASS_PARAM JNIEnv *env, jclass clz

#define JNI_ER_METHOD(return_value, class_name, method_name) JNIEXPORT return_value JNICALL \
    Java_com_johnsoft_testyuv_##class_name##_##method_name

JNI_ER_METHOD(void, YuvConsumer, nativeInit)(JNI_CLASS_PARAM, jint w, jint h) {
    kfWidth = w;
    kfHeight = h;
    video_publisher_start();
}

JNI_ER_METHOD(void, YuvConsumer, nativeFillFrame)(JNI_CLASS_PARAM, jbyteArray array) {
    jbyte *fp = (*env)->GetByteArrayElements(env, array, NULL);
    if (fp) {
        do_x264_encode((uint8_t *) fp);
        (*env)->ReleaseByteArrayElements(env, array, fp, 0);
    }
}
