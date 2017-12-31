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
 * @version 2017-12-27
 */
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <SDL2/SDL.h>
#include <pthread.h>
#include "common.h"
#include "john_synchronized_queue.h"
#include "ffmpeg_base_client.h"
#include "base_path.h"

#define ERROR_BUFFER_SIZE 512
#define PACKET_SIZE 4096

struct FFmpegClient {
    struct SwsContext *sws_context;
    struct SwrContext *swr_context;
    AVFormatContext *format_context;
    int video_stream_index;
    int audio_stream_index;
    AVCodecContext *video_codec_context;
    AVCodecContext *audio_codec_context;
    AVCodec *video_codec;
    AVCodec *audio_codec;
    JohnSynchronizedQueue *video_packet_queue;
    JohnSynchronizedQueue *audio_packet_queue;
    pthread_t video_decode_thread;
    pthread_t audio_decode_thread;
    bool stop_decode;
    VideoOutRule *video_out;
    AudioOutRule *audio_out;
};

FFmpegClient *open_media(const char *url, VideoOutRule *video_out, AudioOutRule *audio_out) {
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
    client->video_stream_index = client->audio_stream_index = -1;
    client->video_out = video_out;
    client->audio_out = audio_out;

    av_register_all();
    avcodec_register_all();

    int result_code;
    if ((result_code = avformat_open_input(&client->format_context, url, NULL, NULL))) {
        char error_buffer[ERROR_BUFFER_SIZE];
        av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
        LOGW("avformat_open_input failed with %s!\n", error_buffer);
        free(client);
        return NULL;
    }

    if (avformat_find_stream_info(client->format_context, NULL) < 0) {
        LOGW("avformat_find_stream_info failed!\n");
        close_media(client);
        return NULL;
    }

    int index;
    if ((index = av_find_best_stream(client->format_context, AVMEDIA_TYPE_VIDEO,
                                     -1, -1, NULL, 0)) < 0) {
        LOGW("not found any video stream!\n");
    } else {
        av_dump_format(client->format_context, index, url, false);
    }
    client->video_stream_index = index;

    if ((index = av_find_best_stream(client->format_context, AVMEDIA_TYPE_AUDIO,
                                     -1, -1, NULL, 0)) < 0) {
        LOGW("not found any audio stream!\n");
    } else {
        av_dump_format(client->format_context, index, url, false);
    }
    client->audio_stream_index = index;

    if (client->video_stream_index < 0 && client->audio_stream_index < 0) {
        close_media(client);
        return NULL;
    }

    if (client->video_stream_index >= 0) {
        AVStream *video_stream = client->format_context->streams[client->video_stream_index];

        client->video_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
        if (!client->video_codec) {
            LOGW("avcodec_find_decoder [video_codec] failed!\n");
            close_media(client);
            return NULL;
        }

        client->video_codec_context = avcodec_alloc_context3(client->video_codec);
        if (!client->video_codec_context) {
            LOGW("avcodec_alloc_context3 [video_codec_context] failed!\n");
            close_media(client);
            return NULL;
        }

        if (avcodec_parameters_to_context(client->video_codec_context,
                                          video_stream->codecpar) < 0) {
            LOGW("avcodec_parameters_to_context [video_codec_context/video_stream] failed!\n");
            close_media(client);
            return NULL;
        }

        av_codec_set_pkt_timebase(client->video_codec_context, video_stream->time_base);

        if (avcodec_open2(client->video_codec_context, client->video_codec, NULL)) {
            LOGW("avcodec_open2 [video_codec_context/video_stream] failed!\n");
            close_media(client);
            return NULL;
        }

        int w = client->video_codec_context->width;
        int h = client->video_codec_context->height;
        enum AVPixelFormat pixel_fmt = client->video_codec_context->pix_fmt;
        if (client->video_out) {
            if (client->video_out->width > 0) {
                w = client->video_out->width;
            } else {
                client->video_out->width = w;
            }
            if (client->video_out->height > 0) {
                h = client->video_out->height;
            } else {
                client->video_out->height = h;
            }
            if (client->video_out->pixel_format != AV_PIX_FMT_NONE) {
                pixel_fmt = client->video_out->pixel_format;
            } else {
                client->video_out->pixel_format = pixel_fmt;
            }
        }
        client->sws_context = sws_getContext(client->video_codec_context->width,
                                             client->video_codec_context->height,
                                             client->video_codec_context->pix_fmt,
                                             w, h, pixel_fmt,
                                             SWS_BICUBIC, NULL, NULL, NULL);
        if (!client->sws_context) {
            LOGW("sws_getContext failed!\n");
            close_media(client);
            return NULL;
        }
    }

    if (client->audio_stream_index >= 0) {
        AVStream *audio_stream = client->format_context->streams[client->audio_stream_index];

        client->audio_codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
        if (!client->audio_codec) {
            LOGW("avcodec_find_decoder [audio_codec] failed!\n");
            close_media(client);
            return NULL;
        }

        client->audio_codec_context = avcodec_alloc_context3(client->audio_codec);
        if (!client->audio_codec_context) {
            LOGW("avcodec_alloc_context3 [audio_codec_context] failed!\n");
            close_media(client);
            return NULL;
        }

        if (avcodec_parameters_to_context(client->audio_codec_context,
                                          audio_stream->codecpar) < 0) {
            LOGW("avcodec_parameters_to_context [audio_codec_context/audio_stream] failed!\n");
            close_media(client);
            return NULL;
        }

        av_codec_set_pkt_timebase(client->audio_codec_context, audio_stream->time_base);

        if (avcodec_open2(client->audio_codec_context, client->audio_codec, NULL)) {
            LOGW("avcodec_open2 [audio_codec_context/audio_stream] failed!\n");
            close_media(client);
            return NULL;
        }

        uint64_t channel_layout = client->audio_codec_context->channel_layout;
        int sample_rate = client->audio_codec_context->sample_rate;
        enum AVSampleFormat sample_fmt = client->audio_codec_context->sample_fmt;
        if (client->audio_out) {
            if (client->audio_out->channel_layout > 0) {
                channel_layout = client->audio_out->channel_layout;
            } else {
                client->audio_out->channel_layout = channel_layout;
            }
            if (client->audio_out->sample_rate > 0) {
                sample_rate = client->audio_out->sample_rate;
            } else {
                client->audio_out->sample_rate = sample_rate;
            }
            if (client->audio_out->sample_format != AV_SAMPLE_FMT_NONE) {
                sample_fmt = client->audio_out->sample_format;
            } else {
                client->audio_out->sample_format = sample_fmt;
            }
        }
        bool swr_success = false;
        if ((client->swr_context = swr_alloc())) {
            if ((client->swr_context = swr_alloc_set_opts(client->swr_context,
                                                         channel_layout, sample_fmt, sample_rate,
                                                         client->audio_codec_context->channel_layout,
                                                         client->audio_codec_context->sample_fmt,
                                                         client->audio_codec_context->sample_rate,
                                                         0, NULL))) {
                if (swr_init(client->swr_context) >= 0) {
                    swr_success = true;
                }
            }
        }
        if (!swr_success) {
            LOGW("SwrContext alloc or init failed!\n");
            close_media(client);
            return NULL;
        }
    }

    return client;
}

void *decode_video_frame(void *data) {
    LOGW("decode_video_frame!\n");
    FFmpegClient *client = (FFmpegClient *) data;
    if (!client) {
        LOGW("client == NULL!\n");
        return NULL;
    }

    int width = client->video_codec_context->width;
    int height = client->video_codec_context->height;
    enum AVPixelFormat pixel_format = client->video_codec_context->pix_fmt;
    if (client->video_out) {
        pixel_format = client->video_out->pixel_format;
        width = client->video_out->width;
        height = client->video_out->height;
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

        int buffer_size = av_image_get_buffer_size(pixel_format, width, height, 1);
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
        frame_result->format = pixel_format;
        frame_result->width = width;
        frame_result->height = height;

        if (av_image_fill_arrays(frame_result->data, frame_result->linesize, buffer,
                                 pixel_format, width, height, 1) < 0) {
            LOGW("av_image_fill_arrays failed!\n");
            error_happened = true;
            goto end_frame;
        }

        if ((result_code = avcodec_send_packet(client->video_codec_context, packet))) {
            char error_buffer[ERROR_BUFFER_SIZE];
            av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
            LOGW("avcodec_send_packet failed with %s!\n", error_buffer);
            goto end_frame;
        }

        double_t pts;
        while ((result_code = avcodec_receive_frame(client->video_codec_context, frame_decoded)) >= 0) {
            if ((pts = av_frame_get_best_effort_timestamp(frame_decoded)) == AV_NOPTS_VALUE) {
                pts = 0;
            }
            pts *= av_q2d(client->format_context->streams[client->video_stream_index]->time_base);

            if ((result_code = sws_scale(client->sws_context, (const uint8_t *const *)frame_decoded->data,
                                         frame_decoded->linesize, 0, client->video_codec_context->height,
                                         frame_result->data, frame_result->linesize)) < 0) {
                char error_buffer[ERROR_BUFFER_SIZE];
                av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
                LOGW("sws_scale failed with %s!\n", error_buffer);
                goto end_frame;
            }

            if (client->video_out && client->video_out->callback) {
                client->video_out->callback(frame_result->data, frame_result->linesize,
                                            (uint32_t) av_get_bits_per_pixel(av_pix_fmt_desc_get(pixel_format)),
                                            (uint32_t) width, (uint32_t) height, (int64_t) (pts * 1000));
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

void *decode_audio_frame(void *data) {
    LOGW("decode_audio_frame!\n");
    FFmpegClient *client = (FFmpegClient *) data;
    if (!client) {
        LOGW("client == NULL!\n");
        return NULL;
    }

    uint64_t channel_layout = client->audio_codec_context->channel_layout;
    int sample_rate = client->audio_codec_context->sample_rate;
    enum AVSampleFormat sample_format = client->audio_codec_context->sample_fmt;
    if (client->audio_out) {
        sample_format = client->audio_out->sample_format;
        channel_layout = client->audio_out->channel_layout;
        sample_rate = client->audio_out->sample_rate;
    }
    int nb_channels = av_get_channel_layout_nb_channels(channel_layout);

    AVFrame *frame_decoded = av_frame_alloc();
    if (!frame_decoded) {
        LOGW("av_frame_alloc failed!\n");
        return NULL;
    }

    int result_code;
    while (!client->stop_decode) {
        AVPacket *packet = (AVPacket *) john_synchronized_queue_dequeue(client->audio_packet_queue, 1000);
        if (!packet) {
            continue;
        }

        AVFrame *frame_result = av_frame_alloc();
        if (!frame_result) {
            LOGW("av_frame_alloc failed!\n");
            goto end_packet;
        }
        frame_result->format = sample_format;
        frame_result->channel_layout = channel_layout;
        frame_result->sample_rate = sample_rate;
        frame_result->channels = nb_channels;

        if ((result_code = avcodec_send_packet(client->audio_codec_context, packet))) {
            char error_buffer[ERROR_BUFFER_SIZE];
            av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
            LOGW("avcodec_send_packet failed with %s!\n", error_buffer);
            goto end_frame;
        }

        double_t pts;
        while ((result_code = avcodec_receive_frame(client->audio_codec_context, frame_decoded)) >= 0) {
            if ((pts = av_frame_get_best_effort_timestamp(frame_decoded)) == AV_NOPTS_VALUE) {
                pts = 0;
            }
            pts *= av_q2d(client->format_context->streams[client->audio_stream_index]->time_base);

            int out_samples1 = (int) av_rescale_rnd(swr_get_delay(client->swr_context, frame_decoded->sample_rate)
                                             + frame_decoded->nb_samples, sample_rate, frame_decoded->sample_rate,
                                             AV_ROUND_UP);
            int out_samples2 = swr_get_out_samples(client->swr_context, frame_decoded->nb_samples);
            frame_result->nb_samples = out_samples2 > out_samples1 ? out_samples2 : out_samples1;

            if ((result_code = swr_convert_frame(client->swr_context, frame_result, frame_decoded)) < 0) {
                char error_buffer[ERROR_BUFFER_SIZE];
                av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
                LOGW("swr_convert failed with %s!\n", error_buffer);
                goto end_frame;
            }

            if (client->audio_out && client->audio_out->callback) {
                client->audio_out->callback(frame_result->data, frame_result->linesize,
                                            (uint32_t) av_get_bytes_per_sample(sample_format),
                                            (uint32_t) nb_channels, (uint32_t) sample_rate,
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
        goto end_packet;

        end_packet:
        av_packet_unref(packet);
        av_packet_free(&packet);
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

    if (pthread_create(&client->video_decode_thread, NULL, decode_video_frame, client)) {
        LOGW("pthread_create [video_decode_thread] failed!\n");
        return;
    }

    if (pthread_create(&client->audio_decode_thread, NULL, decode_audio_frame, client)) {
        LOGW("pthread_create [audio_decode_thread] failed!\n");
        return;
    }

    client->video_packet_queue = john_synchronized_queue_create(400, true, NULL);
    client->audio_packet_queue = john_synchronized_queue_create(400, true, NULL);

    while(true) {
        AVPacket *packet = av_packet_alloc();
        if (!packet) {
            LOGW("av_packet_alloc failed!\n");
            break;
        }
        if (av_new_packet(packet, PACKET_SIZE)) {
            LOGW("av_new_packet failed!\n");
            break;
        }

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
                bak = fopen(BASE_PATH"/temp/x.h264", "ab");
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
        } else if (packet->stream_index == client->audio_stream_index) {
#ifdef BACKUP_STREAM
            static FILE *bak;
            if (!bak) {
                bak = fopen(BASE_PATH"/temp/x.aac", "ab");
            }
            if (bak) {
                fwrite(packet->data, packet->size, 1, bak);
                fflush(bak);
            }
#endif /* BACKUP_STREAM */
#ifndef ONLY_BACKUP
            AVPacket *old_packet = NULL;
            if (!john_synchronized_queue_enqueue(client->audio_packet_queue, packet,
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

    SDL_Delay(1000);

    client->stop_decode = true;
    pthread_join(client->video_decode_thread, NULL);
    pthread_join(client->audio_decode_thread, NULL);
    john_synchronized_queue_destroy(client->video_packet_queue);
    john_synchronized_queue_destroy(client->audio_packet_queue);
}

void close_media(FFmpegClient *client) {
    LOGW("close_media!\n");
    if (client) {
        if (client->video_stream_index >= 0) {
            sws_freeContext(client->sws_context);
            avcodec_close(client->video_codec_context);
            avcodec_free_context(&client->video_codec_context);
        }

        if (client->audio_stream_index >= 0) {
            swr_close(client->swr_context);
            swr_free(&client->swr_context);
            avcodec_close(client->audio_codec_context);
            avcodec_free_context(&client->audio_codec_context);
        }

        avformat_close_input(&client->format_context);
        avformat_free_context(client->format_context);
        free(client);
    }
}

int main(int argc, char **argv) {
    AudioOutRule audioOutRule;
    audioOutRule.sample_format = AV_SAMPLE_FMT_S16;
    audioOutRule.sample_rate = 16000;
    audioOutRule.channel_layout = AV_CH_LAYOUT_MONO;
    FFmpegClient *client = open_media(BASE_PATH"/data/choose-c.mp3", NULL, &audioOutRule);
    if (client) {
        loop_read_frame(client);
        close_media(client);
    }
    return 0;
}