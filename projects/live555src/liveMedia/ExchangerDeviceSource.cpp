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
#include <sys/time.h>
#include "ExchangerDeviceSource.hpp"
#include "common.h"

ExchangerDeviceSource *ExchangerDeviceSource::createNew(UsageEnvironment& env) {
    return new ExchangerDeviceSource(env);
}

ExchangerDataDelegate *ExchangerDeviceSource::dataDelegate = nullptr;
unsigned ExchangerDeviceSource::referenceCount = 0;

ExchangerDeviceSource::ExchangerDeviceSource(UsageEnvironment& env)
        :FramedSource(env), errorHappened(false) {
    if (referenceCount == 0) {
        // Any global initialization of the device would be done here:
    }
    ++referenceCount;
    // Any instance-specific initialization of the device would be done here:
    if (dataDelegate) {
        dataDelegate->onOpen(this);
    }
}

ExchangerDeviceSource::~ExchangerDeviceSource() {
    // Any instance-specific 'destruction' (i.e., resetting) of the device would be done here:
    if (dataDelegate) {
        dataDelegate->onClose(this);
    }
    --referenceCount;
    if (referenceCount == 0) {
        // Any global 'destruction' (i.e., resetting) of the device would be done here:
    }
}

void ExchangerDeviceSource::setErrorHappened(bool errorHappened) {
    this->errorHappened = errorHappened;
}

void ExchangerDeviceSource::doStopGettingFrames() {
    FramedSource::doStopGettingFrames();
}

void ExchangerDeviceSource::doGetNextFrame() {
    if (!dataDelegate || errorHappened /* the source stops being readable */) {
        handleClosure();
        return;
    }

    uint8_t *data;
    uint32_t dataSize;
    if (dataDelegate->readDataSync(this, &data, &dataSize)) {
        LOGW("peeked new data...\n");
        deliverFrame(data, dataSize);
    } else {
        handleClosure();
        return;
    }

    // No new data is immediately available to be delivered.  We don't do anything more here.
    // Instead, our event trigger must be called (e.g., from a separate thread) when new data becomes available.

    LOGW("waiting data...\n");
}

void ExchangerDeviceSource::deliverFrame(uint8_t *data, uint32_t dataSize) {
    // This function is called when new frame data is available from the device.
    // We deliver this data by copying it to the 'downstream' object,
    // using the following parameters (class members):
    // 'in' parameters (these should *not* be modified by this function):
    //     fTo: The frame data is copied to this address.
    //         (Note that the variable "fTo" is *not* modified.  Instead,
    //          the frame data is copied to the address pointed to by "fTo".)
    //     fMaxSize: This is the maximum number of bytes that can be copied
    //         (If the actual frame is larger than this, then it should
    //          be truncated, and "fNumTruncatedBytes" set accordingly.)
    // 'out' parameters (these are modified by this function):
    //     fFrameSize: Should be set to the delivered frame size (<= fMaxSize).
    //     fNumTruncatedBytes: Should be set iff the delivered frame would have been
    //         bigger than "fMaxSize", in which case it's set to the number of bytes
    //         that have been omitted.
    //     fPresentationTime: Should be set to the frame's presentation time
    //         (seconds, microseconds).  This time must be aligned with 'wall-clock time' - i.e.,
    //         the time that you would get by calling "gettimeofday()".
    //     fDurationInMicroseconds: Should be set to the frame's duration, if known.
    //         If, however, the device is a 'live source' (e.g., encoded from a camera or microphone),
    //         then we probably don't need to set this variable,
    //         because - in this case - data will never arrive 'early'.
    // Note the code below.

    if (!isCurrentlyAwaitingData()) return; // we're not ready for the data yet

    if (dataSize <= 0) return;

    // Deliver the data here: fMaxSize[default=1456]
    if (dataSize > fMaxSize) {
        fFrameSize = fMaxSize;
        fNumTruncatedBytes = dataSize - fMaxSize;
    } else {
        fFrameSize = dataSize;
    }
    // If you have a more accurate time - e.g., from an encoder - then use that instead.
    gettimeofday(&fPresentationTime, nullptr);
    // If the device is *not* a 'live source' (e.g., it comes instead from a file or buffer),
    // then set "fDurationInMicroseconds" here.
    memmove(fTo, data, fFrameSize);

    // After delivering the data, inform the reader that it is now available:
    // To avoid possible infinite recursion, we need to return to the event loop to do this:
    nextTask() = envir().taskScheduler()
            .scheduleDelayedTask(0, (TaskFunc*)FramedSource::afterGetting, this);
    LOGW("after getting data...\n");
}
