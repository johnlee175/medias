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
#ifndef EXCHANGER_H264_VIDEO_SERVER_MEDIA_SUBSESSION_H
#define EXCHANGER_H264_VIDEO_SERVER_MEDIA_SUBSESSION_H

#include "OnDemandServerMediaSubsession.hh"
#include "UsageEnvironment.hh"

class ExchangerH264VideoServerMediaSubsession: public OnDemandServerMediaSubsession {
public:
    static unsigned preferBitrate;
public:
    static ExchangerH264VideoServerMediaSubsession *createNew(UsageEnvironment &env, const Boolean &reuseFirstSource);
    // Used to implement "getAuxSDPLine()":
    void checkForAuxSDPLine1();
    void afterPlayingDummy1();
protected:
    ExchangerH264VideoServerMediaSubsession(UsageEnvironment &env, const Boolean &reuseFirstSource);
    ~ExchangerH264VideoServerMediaSubsession() override;

    void setDoneFlag() { fDoneFlag = ~0; }

protected: // redefined virtual functions
    char const* getAuxSDPLine(RTPSink* rtpSink,
                                      FramedSource* inputSource) override;
    FramedSource* createNewStreamSource(unsigned clientSessionId,
                                                unsigned& estBitrate) override;
    RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,
                                      unsigned char rtpPayloadTypeIfDynamic,
                                      FramedSource* inputSource) override;
private:
    char* fAuxSDPLine;
    char fDoneFlag; // used when setting up "fAuxSDPLine"
    RTPSink* fDummyRTPSink; // ditto
};

#endif // EXCHANGER_H264_VIDEO_SERVER_MEDIA_SUBSESSION_H
