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
#include "ExchangerH265VideoServerMediaSubsession.hpp"
#include "ExchangerDeviceSource.hpp"
#include "H265VideoRTPSink.hh"
#include "H265VideoStreamFramer.hh"

ExchangerH265VideoServerMediaSubsession::ExchangerH265VideoServerMediaSubsession(UsageEnvironment &env,
                                                                                 const Boolean &reuseFirstSource)
        : OnDemandServerMediaSubsession(env, reuseFirstSource),
          fAuxSDPLine(nullptr), fDoneFlag(0), fDummyRTPSink(nullptr) {
}

ExchangerH265VideoServerMediaSubsession::~ExchangerH265VideoServerMediaSubsession() {
    delete[] fAuxSDPLine;
}

ExchangerH265VideoServerMediaSubsession *
ExchangerH265VideoServerMediaSubsession::createNew(UsageEnvironment &env,
                                                   const Boolean &reuseFirstSource) {
    return new ExchangerH265VideoServerMediaSubsession(env, reuseFirstSource);
}

unsigned ExchangerH265VideoServerMediaSubsession::preferBitrate = 0;
double ExchangerH265VideoServerMediaSubsession::preferFramerate = -1.0;

static void
afterPlayingDummy(void* clientData) {
    auto* subsess = static_cast<ExchangerH265VideoServerMediaSubsession*>(clientData);
    subsess->afterPlayingDummy1();
}

void
ExchangerH265VideoServerMediaSubsession::afterPlayingDummy1() {
    // Unschedule any pending 'checking' task:
    envir().taskScheduler().unscheduleDelayedTask(nextTask());
    // Signal the event loop that we're done:
    setDoneFlag();
}

static void
checkForAuxSDPLine(void* clientData) {
    auto* subsess = static_cast<ExchangerH265VideoServerMediaSubsession*>(clientData);
    subsess->checkForAuxSDPLine1();
}

void
ExchangerH265VideoServerMediaSubsession::checkForAuxSDPLine1() {
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
ExchangerH265VideoServerMediaSubsession::getAuxSDPLine(RTPSink *rtpSink,
                                                       FramedSource *inputSource) {
    if (fAuxSDPLine != nullptr) return fAuxSDPLine; // it's already been set up (for a previous client)

    if (fDummyRTPSink == nullptr) { // we're not already setting it up for another, concurrent stream
        // Note: For H265 video files, the 'config' information ("profile-level-id" and "sprop-parameter-sets") isn't
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
ExchangerH265VideoServerMediaSubsession::createNewStreamSource(unsigned clientSessionId,
                                                               unsigned &estBitrate) {
    estBitrate = preferBitrate <= 0 ? 500 : preferBitrate; // kbps, estimate
    H265VideoStreamFramer *framer = H265VideoStreamFramer::createNew(envir(),
                                                                     ExchangerDeviceSource::createNew(envir()));
    framer->setFrameRate(preferFramerate <= 0 ? 25.0 : preferFramerate);
    return framer;
}

RTPSink *
ExchangerH265VideoServerMediaSubsession::createNewRTPSink(Groupsock *rtpGroupsock,
                                                          unsigned char rtpPayloadTypeIfDynamic,
                                                          FramedSource *inputSource) {
    return H265VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
}
