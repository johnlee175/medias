/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2017 Live Networks, Inc.  All rights reserved.
// Inclusion of header files representing the interface
// for the entire library
//
// Programs that use the library can include this header file,
// instead of each of the individual media header files

#ifndef _LIVEMEDIA_HH
#define _LIVEMEDIA_HH

#include "MPEG4ESVideoRTPSink.hh"
#include "H264VideoFileSink.hh"
#include "H265VideoFileSink.hh"
#include "BasicUDPSink.hh"
#include "H264VideoRTPSink.hh"
#include "H265VideoRTPSink.hh"
#include "H264VideoStreamFramer.hh"
#include "H265VideoStreamFramer.hh"
#include "H264VideoStreamDiscreteFramer.hh"
#include "H265VideoStreamDiscreteFramer.hh"
#include "SimpleRTPSink.hh"
#include "ByteStreamMultiFileSource.hh"
#include "ByteStreamMemoryBufferSource.hh"
#include "BasicUDPSource.hh"
#include "SimpleRTPSource.hh"
#include "MPEG4LATMAudioRTPSource.hh"
#include "MPEG4LATMAudioRTPSink.hh"
#include "MPEG4ESVideoRTPSource.hh"
#include "MPEG4GenericRTPSource.hh"
#include "H264VideoRTPSource.hh"
#include "H265VideoRTPSource.hh"
#include "MPEG4GenericRTPSink.hh"
#include "MPEG4VideoStreamDiscreteFramer.hh"
#include "DeviceSource.hh"
#include "AudioInputDevice.hh"
#include "StreamReplicator.hh"
#include "RTSPRegisterSender.hh"
#include "RTSPServerSupportingHTTPStreaming.hh"
#include "RTSPClient.hh"
#include "SIPClient.hh"
#include "PassiveServerMediaSubsession.hh"
#include "MPEG4VideoFileServerMediaSubsession.hh"
#include "H264VideoFileServerMediaSubsession.hh"
#include "H265VideoFileServerMediaSubsession.hh"
#include "TCPStreamSink.hh"
#include "ProxyServerMediaSession.hh"

#endif
