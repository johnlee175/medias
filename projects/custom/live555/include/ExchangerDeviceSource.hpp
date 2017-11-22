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
#ifndef _EXCHANGER_DEVICE_SOURCE_H
#define _EXCHANGER_DEVICE_SOURCE_H

#include <cstdint>
#include "UsageEnvironment.hh"
#include "FramedSource.hh"

class ExchangerDeviceSource;

class ExchangerDataDelegate {
public:
    virtual void onOpen(ExchangerDeviceSource *source) = 0;
    virtual bool readDataSync(ExchangerDeviceSource *source, uint8_t **data, uint32_t *dataSize) = 0;
    virtual void onClose(ExchangerDeviceSource *source) = 0;
    virtual ~ExchangerDataDelegate() = default;
};

class ExchangerDeviceSource: public FramedSource {
public:
    static ExchangerDeviceSource *createNew(UsageEnvironment& env);

public:
    static ExchangerDataDelegate *dataDelegate;

public:
    void setErrorHappened(bool errorHappened);

protected:
    // called only by createNew(), or by subclass constructors
    explicit ExchangerDeviceSource(UsageEnvironment& env);
    ~ExchangerDeviceSource() override;

private:
    // redefined virtual functions:
    void doStopGettingFrames() override; // optional
    void doGetNextFrame() override;

private:
    void deliverFrame(uint8_t *data, uint32_t dataSize);

private:
    static unsigned referenceCount; // used to count how many instances of this class currently exist
    bool errorHappened;
};

#endif // _EXCHANGER_DEVICE_SOURCE_H
