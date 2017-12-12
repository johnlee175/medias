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
 * @version 2017-11-14
 */
#include "common.h"
#include "ExchangerDeviceSource.hpp"
#include "ExchangerH264VideoServerMediaSubsession.hpp"
#include "BasicUsageEnvironment.hh"
#include "liveMedia.hh"
#include <x264.h>
#include <pthread.h>
#include <queue>

class ByteArray {
public:
    ByteArray(uint8_t *data, uint32_t size): data(data), size(size) { }
    virtual ~ByteArray() { }
    uint8_t *data;
    uint32_t size;
};

class SyncQueue {
public:
    explicit SyncQueue(int capacity):queue(new std::queue<ByteArray *>()), capacity(capacity) {
        pthread_mutex_init(&mutex, nullptr);
        pthread_cond_init(&cond, nullptr);
    }

    virtual ~SyncQueue() {
        delete queue;
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&cond);
    }

    ByteArray *push(ByteArray *frame) {
        ByteArray *older = nullptr;
        pthread_mutex_lock(&mutex);
        LOGW("queue->size()=%u\n", queue->size());
        while (queue->size() >= capacity) {
            older = queue->front();
            queue->pop();
        }
        queue->push(frame);
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
        return older;
    }

    ByteArray *pop() {
        ByteArray *frame = nullptr;
        pthread_mutex_lock(&mutex);
        LOGW("queue->size()=%u\n", queue->size());
        while (queue->empty()) {
            pthread_cond_wait(&cond, &mutex);
        }
        frame = queue->front();
        queue->pop();
        pthread_mutex_unlock(&mutex);
        return frame;
    }

    size_t size() {
        size_t queue_size = 0;
        pthread_mutex_lock(&mutex);
        queue_size = queue->size();
        pthread_mutex_unlock(&mutex);
        return queue_size;
    }
private:
    int capacity;
    std::queue<ByteArray *> *queue;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

static int kfWidth, kfHeight;
#define FRAME_RATE 12
#define BIT_RATE 500

class MyDataDelegate: public ExchangerDataDelegate {
public:
    MyDataDelegate(): queue(nullptr), closed(true), lastReadData(nullptr),
                      iNal(0), nal(nullptr), picIn(nullptr), picOut(nullptr), h(nullptr) {
#ifdef CONTINUOUS_QUEUE
        doOpen();
#endif // CONTINUOUS_QUEUE
    }
    ~MyDataDelegate() override {
#ifdef CONTINUOUS_QUEUE
        doClose();
#endif // CONTINUOUS_QUEUE
        delete[] lastReadData;
    }

    void onOpen(ExchangerDeviceSource *source) override {
        LOGW("onOpen\n");
#ifndef CONTINUOUS_QUEUE
        doOpen();
#endif // !CONTINUOUS_QUEUE
    }

    bool readDataSync(ExchangerDeviceSource *source, uint8_t **data, uint32_t *size) override {
        LOGW("readDataSync\n");
        if (closed) {
            return false;
        }
        if (lastReadData) {
            delete[] lastReadData;
            lastReadData = nullptr;
        }
        ByteArray *byteArray = queue->pop();
        if (byteArray) {
            *data = byteArray->data;
            *size = byteArray->size;
            delete byteArray;
            lastReadData = *data;
            return true;
        }
        return false;
    }

    void appendYuvData(uint8_t *frame) {
        LOGW("appendYuvData\n");
        if (closed) {
            return;
        }
        if (appendRawFrame(frame) < 0) {
            LOGW("appendRawFrame failed!\n");
        }
    }

    void write264Data(uint8_t *frame, uint32_t size) {
        LOGW("write264Data %u\n", size);
        if (closed) {
            return;
        }
        ByteArray *older = queue->push(new ByteArray(frame, size));
        if (older) {
            delete[] older->data;
            delete older;
        }
    }

    void onClose(ExchangerDeviceSource *source) override {
        LOGW("onClose\n");
        if (closed) {
            return;
        }
#ifndef CONTINUOUS_QUEUE
        doClose();
#endif // !CONTINUOUS_QUEUE
    }

    bool isClosed() {
        return closed;
    }

    size_t getDelay() {
        if (closed) {
            return 0;
        }
        return queue->size() / 100;
    }

private:
    void doOpen() {
        queue = new SyncQueue(500);
        if (createX264Module() < 0) {
            LOGW("createX264Module failed!\n");
        }
        closed = false;
    }

    void doClose() {
        closed = true;
        if (encodeX264Frame() < 0) {
            LOGW("encodeX264Frame failed!\n");
        }
        destroyX264Module();
        ByteArray *array;
        while (queue->size() > 0) {
            array = queue->pop();
            if (array) {
                delete[] array->data;
                delete array;
            }
        }
        delete queue;
    }

    void onEncodedFrame(uint8_t *payload, uint32_t size) {
        uint8_t *data = new uint8_t[size];
        memcpy(data, payload, size);
        write264Data(data, size);
    }

    int createX264Module() {
        x264_param_t param;
        /* Get default params for preset/tuning */
        if (x264_param_default_preset(&param, "fast", "zerolatency") < 0) {
            LOGW("x264_param_default_preset failed!\n");
            return -1;
        }

        /* Configure non-default params */
        const int framerate = FRAME_RATE;
        const int bitrate = BIT_RATE;
        param.i_csp = X264_CSP_YV12;
        param.i_width = kfWidth;
        param.i_height = kfHeight;
        param.i_level_idc = 30;
        param.b_vfr_input = 0;
        param.b_repeat_headers = 1;
        param.b_annexb = 1;
        param.b_aud = 1;
        param.b_cabac = 1;
        param.i_nal_hrd = X264_NAL_HRD_VBR;
        param.rc.b_mb_tree = 0;
        param.rc.f_rf_constant = 25;
        param.rc.f_rf_constant_max = 45;
        param.rc.i_rc_method = X264_RC_ABR;
        param.rc.i_bitrate = bitrate;
        param.rc.i_vbv_max_bitrate = (int) (bitrate * 1.4);
        param.rc.i_vbv_buffer_size = param.rc.i_bitrate;
        param.rc.f_vbv_buffer_init = 0.9;
        param.i_fps_num = (uint32_t) framerate;
        param.i_fps_den = 1;
        param.i_keyint_max = framerate * 2;
        param.i_log_level = X264_LOG_NONE;
        param.rc.f_rate_tolerance = 0.02;
        param.i_frame_reference = 4;

        /* Apply profile restrictions. */
        if (x264_param_apply_profile(&param, "baseline") < 0) {
            LOGW("x264_param_apply_profile failed!\n");
            return -1;
        }

        picIn = new x264_picture_t;
        if (x264_picture_alloc(picIn, param.i_csp, param.i_width, param.i_height) < 0) {
            LOGW("x264_picture_alloc failed!\n");
            return -1;
        }

        picOut = new x264_picture_t;

        h = x264_encoder_open(&param);
        if (!h) {
            LOGW("x264_encoder_open failed!\n");
            x264_picture_clean(picIn);
            return -1;
        }

        return 0;
    }

    int appendRawFrame(uint8_t *frame_data) {
        int luma_size = kfWidth * kfHeight;
        int chroma_size = luma_size / 4;
        memcpy(picIn->img.plane[0], frame_data, luma_size);
        memcpy(picIn->img.plane[1], frame_data + luma_size, chroma_size);
        memcpy(picIn->img.plane[2], frame_data + luma_size + chroma_size, chroma_size);

        ++picIn->i_pts;

        int i_frame_size;
        i_frame_size = x264_encoder_encode(h, &nal, &iNal, picIn, picOut);
        if (i_frame_size < 0) {
            return -1;
        } else if (i_frame_size) {
            onEncodedFrame(nal->p_payload, (uint32_t) i_frame_size);
        }

        return 0;
    }

    int encodeX264Frame() {
        int i_frame_size;
        while (x264_encoder_delayed_frames(h)) {
            i_frame_size = x264_encoder_encode(h, &nal, &iNal, nullptr, picOut);
            if (i_frame_size < 0) {
                return -1;
            } else if (i_frame_size) {
                onEncodedFrame(nal->p_payload, (uint32_t) i_frame_size);
            }
        }
        return 0;
    }

    void destroyX264Module() {
        if (h) {
            x264_encoder_close(h);
            h = nullptr;
        }
        if (picIn) {
            x264_picture_clean(picIn);
            delete picIn;
            picIn = nullptr;
        }
        if (picOut) {
            delete picOut;
            picOut = nullptr;
        }
    }

    bool closed;
    SyncQueue *queue;
    uint8_t *lastReadData;

    x264_picture_t *picIn;
    x264_picture_t *picOut;
    x264_t *h;
    x264_nal_t *nal;
    int iNal;
};

static void do_x264_encode(uint8_t *frame) {
    MyDataDelegate *delegate = static_cast<MyDataDelegate *>(ExchangerDeviceSource::dataDelegate);
    size_t delay;
    if (!delegate->isClosed()) {
        delegate->appendYuvData(frame);
        delay = delegate->getDelay();
        if (delay != 0) {
            usleep(delay * 50000);
        }
    }
}

static void video_server_start() {
    TaskScheduler *scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment *environment = BasicUsageEnvironment::createNew(*scheduler);
    RTSPServer *rtspServer = RTSPServer::createNew(*environment, 8554);

    ExchangerH264VideoServerMediaSubsession::preferBitrate = BIT_RATE;
    ExchangerH264VideoServerMediaSubsession::preferFramerate = FRAME_RATE;
    ExchangerDeviceSource::dataDelegate = new MyDataDelegate();

    ServerMediaSession *sms = ServerMediaSession::createNew(*environment, "testH264", "testH264");
    sms->addSubsession(ExchangerH264VideoServerMediaSubsession::createNew(*environment, False));
    rtspServer->addServerMediaSession(sms);

    char* url = rtspServer->rtspURL(sms);
    LOGW("Play this stream using the URL \"%s\"\n", url);
    delete[] url;

    environment->taskScheduler().doEventLoop();
}

extern "C" {

    #include <jni.h>

    #define JNI_CLASS_PARAM JNIEnv *env, jclass clz

    #define JNI_ER_METHOD(return_value, class_name, method_name) JNIEXPORT return_value JNICALL \
    Java_com_johnsoft_testyuv_##class_name##_##method_name

    JNI_ER_METHOD(void, YuvConsumer, nativeInit)(JNI_CLASS_PARAM, jint w, jint h) {
        kfWidth = w;
        kfHeight = h;
        video_server_start();
    }

    JNI_ER_METHOD(void, YuvConsumer, nativeFillFrame)(JNI_CLASS_PARAM, jbyteArray array) {
        jbyte *fp = env->GetByteArrayElements(array, nullptr);
        if (fp) {
            do_x264_encode((uint8_t *) fp);
            env->ReleaseByteArrayElements(array, fp, 0);
        }
    }

}

