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
#define APPLY_AS_X265
#include <pthread.h>
#include <queue>
#include "common.h"
#include "base_path.h"
#ifdef APPLY_AS_X265
#include "x265_stream.h"
#include "ExchangerH265VideoServerMediaSubsession.hpp"
#else
#include "x264_stream.h"
#include "ExchangerH264VideoServerMediaSubsession.hpp"
#endif
#include "ExchangerDeviceSource.hpp"
#include "BasicUsageEnvironment.hh"
#include "liveMedia.hh"

#ifdef APPLY_AS_X265
#define create_x26x_module create_x265_module
#define append_yuv_frame_x26x append_yuv_frame_x265
#define encode_x26x_frame encode_x265_frame
#define destroy_x26x_module destroy_x265_module
#define X26xStream X265Stream
#define ExchangerH26xVideoServerMediaSubsession ExchangerH265VideoServerMediaSubsession
#define TEST_FRAGMENT "testH265"
#else
#define create_x26x_module create_x264_module
#define append_yuv_frame_x26x append_yuv_frame_x264
#define encode_x26x_frame encode_x264_frame
#define destroy_x26x_module destroy_x264_module
#define X26xStream X264Stream
#define ExchangerH26xVideoServerMediaSubsession ExchangerH264VideoServerMediaSubsession
#define TEST_FRAGMENT "testH264"
#endif

static void on_encoded_frame(uint8_t *payload, uint32_t size, void *user_data);
static void *do_x26x_encode(void *client);

class ByteArray {
public:
    ByteArray(uint8_t *data, uint32_t size): data(data), size(size) { }
    virtual ~ByteArray() = default;
    uint8_t *data;
    uint32_t size;
};

class SyncQueue {
public:
    explicit SyncQueue(int capacity):capacity(capacity), queue(new std::queue<ByteArray *>()) {
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
        LOGW("queue->size()=%lu\n", queue->size());
        while (queue->size() >= (uint32_t) capacity) {
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
        LOGW("queue->size()=%lu\n", queue->size());
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

class MyDataDelegate: public ExchangerDataDelegate {
public:
    MyDataDelegate(): closed(false), stream(nullptr), queue(nullptr),
                      lastReadData(nullptr), pthread(nullptr) { };
    ~MyDataDelegate() override {
        delete[] lastReadData;
    }

    void onOpen(ExchangerDeviceSource *source) override {
        LOGW("onOpen\n");
        queue = new SyncQueue(500);
        if (!(stream = create_x26x_module(width, height, -1, nullptr, nullptr, "zerolatency",
                                          nullptr, on_encoded_frame, this))) {
            LOGW("create_x26x_module failed!\n");
        }
        pthread_create(&pthread, nullptr, do_x26x_encode, this);
        closed = false;
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
        if (append_yuv_frame_x26x(stream, frame) < 0) {
            LOGW("append_yuv_frame_x26x failed!\n");
        }
    }

    void write26xData(uint8_t *frame, uint32_t size) {
        LOGW("write26xData %u\n", size);
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
        closed = true;
        pthread_join(pthread, nullptr);
        if (encode_x26x_frame(stream) < 0) {
            LOGW("encode_x26x_frame failed!\n");
        }
        destroy_x26x_module(stream);
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
    bool closed;
    X26xStream *stream;
    SyncQueue *queue;
    uint8_t *lastReadData;
    pthread_t pthread;
    static const int width = 512, height = 288;
};

static void on_encoded_frame(uint8_t *payload, uint32_t size, void *user_data) {
    uint8_t *data = new uint8_t[size];
    memcpy(data, payload, size);
    dynamic_cast<MyDataDelegate *>(ExchangerDeviceSource::dataDelegate)->write26xData(data, size);
}

static void *do_x26x_encode(void *client) {
    MyDataDelegate *delegate = static_cast<MyDataDelegate *>(client);
    // ffmpeg -i data/test.mp4 -c:v rawvideo -pix_fmt yuv420p data/test_512x288.yuv
    const char *yuv_file = BASE_PATH"/data/test_512x288.yuv";
    size_t frame_size = 512 * 288 * 3 / 2;
    uint8_t *frame = new uint8_t[frame_size];
    FILE *file = fopen(yuv_file, "r");
    size_t delay;
    while (!delegate->isClosed() && fread(frame, 1, frame_size, file) == frame_size) {
        delegate->appendYuvData(frame);
        delay = delegate->getDelay();
        if (delay != 0) {
            usleep(static_cast<useconds_t>(delay * 50000));
        }
    }
    return nullptr;
}

static void video_server_start() {
    TaskScheduler *scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment *environment = BasicUsageEnvironment::createNew(*scheduler);
    RTSPServer *rtspServer = RTSPServer::createNew(*environment, 8554);

    ExchangerDeviceSource::dataDelegate = new MyDataDelegate();
    ExchangerH26xVideoServerMediaSubsession::preferBitrate = 100; // kbps
    ExchangerH26xVideoServerMediaSubsession::preferFramerate = 24.0;

    ServerMediaSession *sms = ServerMediaSession::createNew(*environment, TEST_FRAGMENT, TEST_FRAGMENT);
    sms->addSubsession(ExchangerH26xVideoServerMediaSubsession::createNew(*environment, False));
    rtspServer->addServerMediaSession(sms);

    char* url = rtspServer->rtspURL(sms);
    *environment << "Play this stream using the URL \"" << url << "\"\n";
    delete[] url;

    environment->taskScheduler().doEventLoop();
}

#include "vpx/vp8.h"

int main(int argc, char **argv) {
    LOGW("vpx_codec_version %d\n", vpx_codec_version()); // make sure libvpx.a work
    video_server_start();
    return 0;
}
