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
#include "ExchangerH264VideoServerMediaSubsession.hpp"
#include "ExchangerDeviceSource.hpp"
#include "H264VideoRTPSink.hh"
#include "H264VideoStreamFramer.hh"

ExchangerH264VideoServerMediaSubsession::ExchangerH264VideoServerMediaSubsession(UsageEnvironment &env,
                                                                                 const Boolean &reuseFirstSource)
        : OnDemandServerMediaSubsession(env, reuseFirstSource),
          fAuxSDPLine(nullptr), fDoneFlag(0), fDummyRTPSink(nullptr) {
}

ExchangerH264VideoServerMediaSubsession::~ExchangerH264VideoServerMediaSubsession() {
    delete[] fAuxSDPLine;
}

ExchangerH264VideoServerMediaSubsession *
ExchangerH264VideoServerMediaSubsession::createNew(UsageEnvironment &env,
                                                   const Boolean &reuseFirstSource) {
    return new ExchangerH264VideoServerMediaSubsession(env, reuseFirstSource);
}

unsigned ExchangerH264VideoServerMediaSubsession::preferBitrate = 0;
double ExchangerH264VideoServerMediaSubsession::preferFramerate = -1.0;

static void
afterPlayingDummy(void* clientData) {
    auto* subsess = static_cast<ExchangerH264VideoServerMediaSubsession*>(clientData);
    subsess->afterPlayingDummy1();
}

void
ExchangerH264VideoServerMediaSubsession::afterPlayingDummy1() {
    // Unschedule any pending 'checking' task:
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
    // Signal the event loop that we're done:
    setDoneFlag();
}

static void
checkForAuxSDPLine(void* clientData) {
    auto* subsess = static_cast<ExchangerH264VideoServerMediaSubsession*>(clientData);
    subsess->checkForAuxSDPLine1();
}

void
ExchangerH264VideoServerMediaSubsession::checkForAuxSDPLine1() {
    nextTask() = nullptr;

    char const* dasl;
    if (fAuxSDPLine != nullptr) {
        // Signal the event loop that we're done:
        setDoneFlag();
    } else if (fDummyRTPSink != nullptr && (dasl = fDummyRTPSink->auxSDPLine()) != nullptr) {
        fAuxSDPLine = strDup(dasl);
        fDummyRTPSink = nullptr;

        // Signal the event loop that we're done:
        setDoneFlag();
    } else if (!fDoneFlag) {
        // try again after a brief delay:
        int uSecsToDelay = 100000; // 100 ms
        nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecsToDelay,
                                                                 (TaskFunc*)checkForAuxSDPLine, this);
    }
}

char const *
ExchangerH264VideoServerMediaSubsession::getAuxSDPLine(RTPSink *rtpSink,
                                                       FramedSource *inputSource) {
    if (fAuxSDPLine != nullptr) return fAuxSDPLine; // it's already been set up (for a previous client)

    if (fDummyRTPSink == nullptr) { // we're not already setting it up for another, concurrent stream
        // Note: For H264 video files, the 'config' information ("profile-level-id" and "sprop-parameter-sets") isn't
        // known until we start reading the file.  This means that "rtpSink"s "auxSDPLine()" will be nullptr initially,
        // and we need to start reading data from our file until this changes.
        fDummyRTPSink = rtpSink;

        // Start reading the file:
        envir() << "startPlaying for SPS PPS...\n";
        fDummyRTPSink->startPlaying(*inputSource, afterPlayingDummy, this);

        // Check whether the sink's 'auxSDPLine()' is ready:
        checkForAuxSDPLine(this);
    }

    envir().taskScheduler().doEventLoop(&fDoneFlag);
    envir() << "finish getAuxSDPLine..." << fAuxSDPLine << "\n";
    return fAuxSDPLine;
}

FramedSource *
ExchangerH264VideoServerMediaSubsession::createNewStreamSource(unsigned clientSessionId,
                                                               unsigned &estBitrate) {
    estBitrate = preferBitrate <= 0 ? 500 : preferBitrate; // kbps, estimate
    H264VideoStreamFramer *framer = H264VideoStreamFramer::createNew(envir(),
                                                                     ExchangerDeviceSource::createNew(envir()));
    framer->setFrameRate(preferFramerate <= 0 ? 25.0 : preferFramerate);
    return framer;
}

RTPSink *
ExchangerH264VideoServerMediaSubsession::createNewRTPSink(Groupsock *rtpGroupsock,
                                                          unsigned char rtpPayloadTypeIfDynamic,
                                                          FramedSource *inputSource) {
    return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
}
