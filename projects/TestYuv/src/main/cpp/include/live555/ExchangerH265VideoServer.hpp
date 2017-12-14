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
#ifndef EXCHANGER_H265_VIDEO_SERVER_H
#define EXCHANGER_H265_VIDEO_SERVER_H

#include <unistd.h>
#include "ExchangerDeviceSource.hpp"
#include "ExchangerH265VideoServerMediaSubsession.hpp"
#include "BasicUsageEnvironment.hh"
#include "liveMedia.hh"
#include "common.h"

class SimpleDataDelegate: public ExchangerDataDelegate {
public:
    SimpleDataDelegate(const char *filePath, size_t bufferSize)
            : file(nullptr), buffer(new uint8_t[bufferSize]),
              filePath(filePath), bufferSize(bufferSize) { }

    ~SimpleDataDelegate() override {
        delete[] buffer;
    }

    void onOpen(ExchangerDeviceSource *source) override {
        file = fopen(filePath, "r");
        LOGW("onOpen\n");
    }

    bool readDataSync(ExchangerDeviceSource *source, uint8_t **data, uint32_t *dataSize) override {
        size_t readedCount;
        if (file && !feof(file)) {
            LOGW("reading data...\n");
            readedCount = fread(buffer, sizeof(uint8_t), bufferSize, file);
            if (readedCount > 0) {
                *data = buffer;
                *dataSize = static_cast<uint32_t>(readedCount);
                return true;
            }
        }
        return false;
    }

    void onClose(ExchangerDeviceSource *source) override {
        if (file) {
            LOGW("onClose\n");
            fclose(file);
        }
    }
private:
    FILE *file;
    uint8_t *buffer;
    const char *filePath;
    size_t bufferSize;
};

static void ExchangerH265VideoServer_doRtsp() {
    TaskScheduler *scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment *environment = BasicUsageEnvironment::createNew(*scheduler);
    RTSPServer *rtspServer = RTSPServer::createNew(*environment, 8554);

    ExchangerDeviceSource::dataDelegate = new SimpleDataDelegate(
            "data/test.265", 1024);

    ServerMediaSession *sms = ServerMediaSession::createNew(*environment, "testH265", "testH265");
    sms->addSubsession(ExchangerH265VideoServerMediaSubsession::createNew(*environment, False));
    rtspServer->addServerMediaSession(sms);

    char* url = rtspServer->rtspURL(sms);
    *environment << "Play this stream using the URL \"" << url << "\"\n";
    delete[] url;

    environment->taskScheduler().doEventLoop();
}

#endif // EXCHANGER_H265_VIDEO_SERVER_H
