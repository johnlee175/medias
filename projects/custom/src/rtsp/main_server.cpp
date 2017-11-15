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
#include "x264_stream.h"
#include "ExchangerDeviceSource.hpp"
#include "ExchangerH264VideoServerMediaSubsession.hpp"
#include "BasicUsageEnvironment.hh"
#include "liveMedia.hh"
#include <pthread.h>
#include <queue>

static void on_encoded_frame(uint8_t *payload, uint32_t size);
static void *do_x264_encode(void *client);

class ByteArray {
public:
    ByteArray(uint8_t *data, uint32_t size): data(data), size(size) { }
    virtual ~ByteArray() = default;
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
        LOGW("queue->size()=%lu\n", queue->size());
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
    MyDataDelegate(): stream(nullptr), queue(nullptr), closed(false), lastReadData(nullptr) { };
    ~MyDataDelegate() override {
        delete[] lastReadData;
    }

    void onOpen(ExchangerDeviceSource *source) override {
        LOGW("onOpen\n");
        closed = false;
        queue = new SyncQueue(500);
        if (!(stream = create_x264_module(width, height, nullptr, nullptr, on_encoded_frame))) {
            LOGW("create_x264_module failed!\n");
        }
        pthread_create(&pthread, nullptr, do_x264_encode, this);
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
        if (append_i420_frame(stream, frame) < 0) {
            LOGW("append_i420_frame failed!\n");
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
        closed = true;
        pthread_join(pthread, nullptr);
        if (encode_x264_frame(stream) < 0) {
            LOGW("encode_x264_frame failed!\n");
        }
        destroy_x264_module(stream);
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
    pthread_t pthread;
    X264Stream *stream;
    SyncQueue *queue;
    uint8_t *lastReadData;
    static const int width = 512, height = 288;
};

static void on_encoded_frame(uint8_t *payload, uint32_t size) {
    uint8_t *data = new uint8_t[size];
    memcpy(data, payload, size);
    dynamic_cast<MyDataDelegate *>(ExchangerDeviceSource::dataDelegate)->write264Data(data, size);
}

static void *do_x264_encode(void *client) {
    MyDataDelegate *delegate = static_cast<MyDataDelegate *>(client);
    // ffmpeg -i data/cuc_ieschool.mp4 -c:v rawvideo -pix_fmt yuv420p data/cuc_ieschool.yuv
    const char *yuv_file = "data/cuc_ieschool.yuv";
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

    ServerMediaSession *sms = ServerMediaSession::createNew(*environment, "testH264", "testH264");
    sms->addSubsession(ExchangerH264VideoServerMediaSubsession::createNew(*environment, False));
    rtspServer->addServerMediaSession(sms);

    char* url = rtspServer->rtspURL(sms);
    *environment << "Play this stream using the URL \"" << url << "\"\n";
    delete[] url;

    environment->taskScheduler().doEventLoop();
}

int main(int argc, char **argv) {
    video_server_start();
    return 0;
}
