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
#include <pthread.h>
#include "common.h"
#include "ffmpeg_url_client.h"
#include "john_synchronized_queue.h"

#define ERROR_BUFFER_SIZE 512

struct FFmpegClient {
    FrameCallback frame_callback;
    enum AVPixelFormat pixel_format;
    AVFormatContext *format_context;
    int video_stream_index;
    AVCodecContext *codec_context;
    AVCodec *codec;
    struct SwsContext *sws_context;
    JohnSynchronizedQueue *video_packet_queue;
    pthread_t decode_thread;
    bool stop_decode;
};

FFmpegClient *open_media(const char *url, enum AVPixelFormat pixel_format, FrameCallback frame_callback) {
    LOGW("open_media!\n");
    if (!url) {
        LOGW("url == NULL!\n");
        return NULL;
    }

    FFmpegClient *client = (FFmpegClient *) malloc(sizeof(FFmpegClient));
    if (!client) {
        LOGW("malloc FFmpegClient failed!\n");
        return NULL;
    }
    memset(client, 0, sizeof(FFmpegClient));

    av_register_all();
    avcodec_register_all();

    client->pixel_format = pixel_format;
    client->frame_callback = frame_callback;

    avformat_network_init();

    AVDictionary* options = NULL;
    av_dict_set(&options, "buffer_size", "3276800", 0); /* libavformat/udp.c */
    av_dict_set(&options, "pkt_size", "10240", 0); /* libavformat/udp.c */
    av_dict_set(&options, "fifo_size", "655350", 0); /* libavformat/udp.c */
    av_dict_set(&options, "send_buffer_size", "16384", 0); /* libavformat/tcp.c small size for small play delay */
    av_dict_set(&options, "recv_buffer_size", "16384", 0); /* libavformat/tcp.c small size for small play delay */
    av_dict_set(&options, "reorder_queue_size", "2000", 0); /* libavformat/rtsp.c */
    av_dict_set(&options, "stimeout", "15000000", 0); /* libavformat/rtsp.c */
    av_dict_set(&options, "rtsp_transport", "tcp", 0); /* libavformat/rtsp.c */
    av_dict_set(&options, "rtmp_buffer", "1000", 0); /* libavformat/rtmpproto.c small size for small play delay */
    av_dict_set(&options, "max_delay", "1000000", 0); /* libavformat/options_table.h */
    av_dict_set(&options, "packetsize", "10240", 0); /* libavformat/options_table.h */
    av_dict_set(&options, "rtbufsize", "11059200", 0); /* libavformat/options_table.h */

    int result_code;
    if ((result_code = avformat_open_input(&client->format_context, url, NULL, &options))) {
        char error_buffer[ERROR_BUFFER_SIZE];
        av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
        LOGW("avformat_open_input failed with %s!\n", error_buffer);
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

void *decode_stream_frame(void *data) {
    LOGW("decode_stream_frame!\n");
    FFmpegClient *client = (FFmpegClient *) data;
    if (!client) {
        LOGW("client == NULL!\n");
        return NULL;
    }

    AVFrame *frame_decoded = av_frame_alloc();
    if (!frame_decoded) {
        LOGW("av_frame_alloc failed!\n");
        return NULL;
    }

    int result_code;
    bool error_happened;
    while (!client->stop_decode) {
        error_happened = false;
        AVPacket *packet = (AVPacket *) john_synchronized_queue_dequeue(client->video_packet_queue, 1000);
        if (!packet) {
            continue;
        }

        int buffer_size = av_image_get_buffer_size(client->pixel_format, client->codec_context->width,
                                                   client->codec_context->height, 1);
        if (buffer_size <= 0) {
            LOGW("av_image_get_buffer_size failed!\n");
            error_happened = true;
            goto end_packet;
        }
        uint8_t *buffer = (uint8_t *)av_malloc((size_t) buffer_size);
        if (!buffer) {
            LOGW("av_malloc failed!\n");
            error_happened = true;
            goto end_packet;
        }

        AVFrame *frame_result = av_frame_alloc();
        if (!frame_result) {
            LOGW("av_frame_alloc failed!\n");
            error_happened = true;
            goto end_buffer;
        }
        frame_result->format = client->pixel_format;
        frame_result->width = client->codec_context->width;
        frame_result->height = client->codec_context->height;

        if (av_image_fill_arrays(frame_result->data, frame_result->linesize, buffer, client->pixel_format,
                                 client->codec_context->width, client->codec_context->height, 1) < 0) {
            LOGW("av_image_fill_arrays failed!\n");
            error_happened = true;
            goto end_frame;
        }

        if ((result_code = avcodec_send_packet(client->codec_context, packet))) {
            char error_buffer[ERROR_BUFFER_SIZE];
            av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
            LOGW("avcodec_send_packet failed with %s!\n", error_buffer);
            goto end_frame;
        }

        double_t pts;
        while ((result_code = avcodec_receive_frame(client->codec_context, frame_decoded)) >= 0) {
            if ((pts = av_frame_get_best_effort_timestamp(frame_decoded)) == AV_NOPTS_VALUE) {
                pts = 0;
            }
            pts *= av_q2d(client->format_context->streams[client->video_stream_index]->time_base);

            if ((result_code = sws_scale(client->sws_context, (const uint8_t *const *)frame_decoded->data,
                                         frame_decoded->linesize, 0, client->codec_context->height,
                                         frame_result->data, frame_result->linesize)) < 0) {
                char error_buffer[ERROR_BUFFER_SIZE];
                av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
                LOGW("sws_scale failed with %s!\n", error_buffer);
                goto end_frame;
            }

            if (client->frame_callback) {
                client->frame_callback(frame_result->data, frame_result->linesize,
                                       (uint32_t) client->codec_context->width,
                                       (uint32_t) client->codec_context->height,
                                       (int64_t) (pts * 1000));
            }
        }
        if (result_code != -EAGAIN) {
            char error_buffer[ERROR_BUFFER_SIZE];
            av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
            LOGW("avcodec_receive_frame result with %s\n", error_buffer);
        }
        goto end_frame;

        end_frame:
        av_frame_free(&frame_result);
        goto end_buffer;

        end_buffer:
        av_free(buffer);
        goto end_packet;

        end_packet:
        av_packet_unref(packet);
        av_packet_free(&packet);
        if (error_happened) {
            break;
        }
    }

    av_frame_free(&frame_decoded);
    return NULL;
}

void loop_read_frame(FFmpegClient *client) {
    LOGW("loop_read_frame!\n");
    if (!client) {
        LOGW("client == NULL!\n");
        return;
    }

    if (pthread_create(&client->decode_thread, NULL, decode_stream_frame, client)) {
        LOGW("pthread_create failed!\n");
        return;
    }

    client->video_packet_queue = john_synchronized_queue_create(400, true, NULL);

    while(true) {
        AVPacket *packet = av_packet_alloc();
        if (!packet) {
            LOGW("av_packet_alloc failed!\n");
            break;
        }
        av_init_packet(packet);

        int result_code;
        if ((result_code = av_read_frame(client->format_context, packet)) < 0) {
            char error_buffer[ERROR_BUFFER_SIZE];
            av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
            LOGW("av_read_frame result with %s\n", error_buffer);
            break;
        }

        if (packet->stream_index == client->video_stream_index) {
#ifdef BACKUP_STREAM
            static FILE *bak;
            if (!bak) {
                bak = fopen("temp/x.h264", "ab");
            }
            if (bak) {
                fwrite(packet->data, packet->size, 1, bak);
                fflush(bak);
            }
#endif /* BACKUP_STREAM */
#ifndef ONLY_BACKUP
            AVPacket *old_packet = NULL;
            if (!john_synchronized_queue_enqueue(client->video_packet_queue, packet,
                                                 (void **) &old_packet, -1)) {
                LOGW("john_synchronized_queue_enqueue failed!\n");
                break;
            }

            if (old_packet) {
                av_packet_unref(old_packet);
                av_packet_free(&old_packet);
            }
#endif /* ONLY_BACKUP */
        }
    }

    client->stop_decode = true;
    pthread_join(client->decode_thread, NULL);
    john_synchronized_queue_destroy(client->video_packet_queue);
}

void close_media(FFmpegClient *client) {
    LOGW("close_media!\n");
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
#include "ffmpeg_url_client.h"
#include "base_path.h"
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

    FFmpegClient *client = open_media(BASE_PATH"/data/test.mp4",
                                   AV_PIX_FMT_BGR24, on_frame_mp4);
    if (client) {
        loop_read_frame(client);
        close_media(client);
    }
}
*/
