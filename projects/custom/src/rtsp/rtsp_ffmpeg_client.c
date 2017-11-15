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
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include "common.h"
#include "rtsp_ffmpeg_client.h"

struct RtspClient {
    FrameCallback frame_callback;
    enum AVPixelFormat pixel_format;
    AVFormatContext *format_context;
    int video_stream_index;
    AVCodecContext *codec_context;
    AVCodec *codec;
    struct SwsContext *sws_context;
};

RtspClient *open_rtsp(const char *rtsp_url, enum AVPixelFormat pixel_format, FrameCallback frame_callback) {
    LOGW("open_rtsp!\n");
    if (!rtsp_url) {
        LOGW("rtsp_url == NULL!\n");
        return NULL;
    }

    RtspClient *client = (RtspClient *) malloc(sizeof(RtspClient));
    if (!client) {
        LOGW("malloc RtspClient failed!\n");
        return NULL;
    }
    memset(client, 0, sizeof(RtspClient));

    av_register_all();
    avcodec_register_all();

    client->pixel_format = pixel_format;
    client->frame_callback = frame_callback;

    avformat_network_init();
    if (avformat_open_input(&client->format_context, rtsp_url, NULL, NULL)) {
        LOGW("avformat_open_input failed!\n");
        return NULL;
    }

    if (avformat_find_stream_info(client->format_context, NULL) < 0) {
        LOGW("avformat_find_stream_info failed!\n");
        return NULL;
    }

    int index;
    if ((index = av_find_best_stream(client->format_context, AVMEDIA_TYPE_VIDEO,
                                     -1, -1, NULL, 0)) < 0) {
        LOGW("not found any video stream!\n");
        return NULL;
    }
    client->video_stream_index = index;

    AVStream *video_stream = client->format_context->streams[index];

    client->codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!client->codec) {
        LOGW("avcodec_find_decoder failed!\n");
        return NULL;
    }

    client->codec_context = avcodec_alloc_context3(client->codec);
    if (!client->codec_context) {
        LOGW("avcodec_alloc_context3 failed!\n");
        return NULL;
    }
    if (avcodec_parameters_to_context(client->codec_context,
                                      video_stream->codecpar) < 0) {
        LOGW("avcodec_parameters_to_context failed!\n");
        return NULL;
    }
    av_codec_set_pkt_timebase(client->codec_context, video_stream->time_base);

    if (avcodec_open2(client->codec_context, client->codec, NULL)) {
        LOGW("avcodec_open2 failed!\n");
        return NULL;
    }

    client->sws_context = sws_getContext(client->codec_context->width, client->codec_context->height,
                                         client->codec_context->pix_fmt, client->codec_context->width,
                                         client->codec_context->height, client->pixel_format,
                                         SWS_BICUBIC, NULL, NULL, NULL);
    if (!client->sws_context) {
        LOGW("sws_getContext failed!\n");
        return NULL;
    }

    return client;
}

void loop_read_rtsp_frame(RtspClient *client) {
    LOGW("loop_read_rtsp_frame!\n");
    if (!client) {
        LOGW("client == NULL!\n");
        return;
    }

    AVFrame *frame_decoded = av_frame_alloc();
    if (!frame_decoded) {
        LOGW("av_frame_alloc failed!\n");
        return;
    }
    AVFrame *frame_result = av_frame_alloc();
    if (!frame_result) {
        LOGW("av_frame_alloc failed!\n");
        return;
    }

    frame_result->format = client->pixel_format;
    frame_result->width = client->codec_context->width;
    frame_result->height = client->codec_context->height;
    int buffer_size = av_image_get_buffer_size(client->pixel_format, client->codec_context->width,
                                               client->codec_context->height, 1);
    if (buffer_size <= 0) {
        LOGW("av_image_get_buffer_size failed!\n");
        return;
    }
    uint8_t *buffer = (uint8_t *)av_malloc((size_t) buffer_size);
    if (!buffer) {
        LOGW("av_malloc failed!\n");
        return;
    }
    if (av_image_fill_arrays(frame_result->data, frame_result->linesize, buffer, client->pixel_format,
                             client->codec_context->width, client->codec_context->height, 1) < 0) {
        LOGW("av_image_fill_arrays failed!\n");
        return;
    }

    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        LOGW("av_packet_alloc failed!\n");
        return;
    }
    if (av_new_packet(packet, client->codec_context->width * client->codec_context->height)) {
        LOGW("av_new_packet failed!\n");
        return;
    }

    while (av_read_frame(client->format_context, packet) >= 0) {
        if (packet->stream_index == client->video_stream_index) {
            if (avcodec_send_packet(client->codec_context, packet)) {
                LOGW("avcodec_send_packet failed!\n");
                goto end;
            }

            double_t pts;

            while (avcodec_receive_frame(client->codec_context, frame_decoded) >= 0) {
                if ((pts = av_frame_get_best_effort_timestamp(frame_decoded)) == AV_NOPTS_VALUE) {
                    pts = 0;
                }
                pts *= av_q2d(client->format_context->streams[client->video_stream_index]->time_base);

                if (sws_scale(client->sws_context, (const uint8_t *const *)frame_decoded->data,
                              frame_decoded->linesize, 0, client->codec_context->height,
                              frame_result->data, frame_result->linesize) < 0) {
                    LOGW("sws_scale failed!\n");
                    goto end;
                }

                if (client->frame_callback) {
                    client->frame_callback(frame_result->data, frame_result->linesize,
                                           (uint32_t) client->codec_context->width,
                                           (uint32_t) client->codec_context->height,
                                           (int64_t) (pts * 1000));
                }
            }
        }
        av_packet_unref(packet);
    }

    end:
    av_free(buffer);
    av_frame_free(&frame_decoded);
    av_frame_free(&frame_result);
    av_packet_free(&packet);
}

void close_rtsp(RtspClient *client) {
    LOGW("close_rtsp!\n");
    if (client) {
        sws_freeContext(client->sws_context);
        avcodec_close(client->codec_context);
        avcodec_free_context(&client->codec_context);
        avformat_close_input(&client->format_context);
        avformat_free_context(client->format_context);
        free(client);
    }
}

/* ====================== testing ====================== */

/*
#include "rtsp_ffmpeg_client.h"
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

static int64_t base_clock_millis = 0;

static void on_frame_mp4(uint8_t *data[8], int line_size[8],
                     uint32_t width, uint32_t height, int64_t pts_millis) {
    cv::Mat image(height, width, CV_8UC3, data[0]);
    cv::imshow("Image Window", image);
    int duration = (int)(pts_millis - base_clock_millis);
    if (duration <= 0) {
        duration = 1;
    }
    cv::waitKey(duration);
    base_clock_millis = pts_millis;
}

static void test_mp4() {
    cv::namedWindow("Image Window", cv::WINDOW_AUTOSIZE);

    RtspClient *client = open_rtsp("data/cuc_ieschool.mp4",
                                   AV_PIX_FMT_BGR24, on_frame_mp4);
    if (client) {
        loop_read_rtsp_frame(client);
        close_rtsp(client);
    }
}
*/
