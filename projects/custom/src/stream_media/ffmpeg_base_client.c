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
#include <libavutil/parseutils.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <pthread.h>
#include "base_path.h"
#include "common.h"
#include "john_synchronized_queue.h"
#include "ffmpeg_base_client.h"
#include "trace_malloc_free.h"

static int thread_sleep(pthread_mutex_t *mutex, pthread_cond_t *cond, int64_t timeout_millis) {
    int result_code = -1;
    if (mutex && cond && timeout_millis > 0) {
        if (!pthread_mutex_lock(mutex)) {
            struct timespec spec;
            if (!clock_gettime(CLOCK_REALTIME, &spec)) {
                spec.tv_sec += timeout_millis / 1000;
                spec.tv_nsec += (timeout_millis % 1000) * 1000000;
                while (spec.tv_nsec > 1000000000) {
                    spec.tv_sec += 1;
                    spec.tv_nsec -= 1000000000;
                }
                int rc;
                if ((rc = pthread_cond_timedwait(cond, mutex, &spec)) == ETIMEDOUT) {
                    result_code = 0;
                } else {
                    LOGW("pthread_cond_timedwait != ETIMEDOUT, is %d!\n", rc);
                }
            } else {
                LOGW("call clock_gettime failed!\n");
            }
            pthread_mutex_unlock(mutex);
        } else {
            LOGW("call pthread_mutex_lock failed!\n");
        }
    } else {
        LOGW("mutex or cond is NULL, or timeout_millis <= 0!\n");
    }
    return result_code;
}

static int64_t system_current_millis() {
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    while (spec.tv_nsec > 1000000000) {
        spec.tv_sec += 1;
        spec.tv_nsec -= 1000000000;
    }
    return (spec.tv_sec * 1000000000 + spec.tv_nsec) / 1000000;
}

#define ERROR_BUFFER_SIZE 512
#define PACKET_SIZE 4096
#define QUEUE_SIZE 500
#define SEEK_THRESHOLD 1000

typedef struct VideoCallbackData {
    uint8_t **data;
    int *line_size;
    uint32_t pixel_precision;
    uint32_t width;
    uint32_t height;
    int64_t pts_millis;
    void *user_tag;
} VideoCallbackData;

typedef struct AudioCallbackData {
    uint8_t **data;
    int *line_size;
    uint32_t sample_precision;
    uint32_t channels;
    uint32_t sample_rate;
    int64_t pts_millis;
    void *user_tag;
} AudioCallbackData;

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
    JohnSynchronizedQueue *video_frame_queue;
    JohnSynchronizedQueue *audio_frame_queue;
    pthread_t video_decode_thread;
    pthread_t audio_decode_thread;
    pthread_t video_play_thread;
    pthread_t audio_play_thread;
    bool stop_decode;
    bool stop_video_play;
    bool stop_audio_play;
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
            client->video_out->has_video = true;
            if (client->video_out->width > 0 && client->video_out->height > 0) {
                w = client->video_out->width;
                h = client->video_out->height;
            } else if (client->video_out->width > 0) {
                w = client->video_out->width;
                h = (int) (client->video_out->width *
                           (client->video_codec_context->height / (float) client->video_codec_context->width));
                client->video_out->height = h;
            } else if (client->video_out->height > 0) {
                h = client->video_out->height;
                w = (int) (client->video_out->height *
                        (client->video_codec_context->width / (float) client->video_codec_context->height));
                client->video_out->width = w;
            } else {
                client->video_out->width = w;
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

        if (client->audio_codec_context->channel_layout == 0
            && client->audio_codec_context->channels > 0) {
            client->audio_codec_context->channel_layout =
                    (uint64_t) av_get_default_channel_layout(client->audio_codec_context->channels);
        }

        uint64_t channel_layout = client->audio_codec_context->channel_layout;
        int sample_rate = client->audio_codec_context->sample_rate;
        enum AVSampleFormat sample_fmt = client->audio_codec_context->sample_fmt;
        if (client->audio_out) {
            client->audio_out->has_audio = true;
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

int64_t get_pts_time_base(FFmpegClient *client, AVFrame *frame_decoded, int stream_index) {
    int64_t pts = 0;
    if (frame_decoded->best_effort_timestamp != AV_NOPTS_VALUE) {
        pts = frame_decoded->best_effort_timestamp;
    } else if (frame_decoded->pts != AV_NOPTS_VALUE) {
        pts = frame_decoded->pts;
    } else {
        pts = AV_NOPTS_VALUE;
    }
    if (pts != AV_NOPTS_VALUE) {
        pts = (int64_t) (pts * av_q2d(client->format_context->streams[stream_index]->time_base) * 1000L);
    }
    return pts;
}

void alloc_fill_from_frame(uint8_t ***data, int **line_size, const AVFrame *frame_result) {
    *data = (uint8_t **) malloc(sizeof(uint8_t *) * AV_NUM_DATA_POINTERS);
    if (*data) {
        memset(*data, 0, sizeof(uint8_t *) * AV_NUM_DATA_POINTERS);
        size_t sz;
        for (int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
            if (frame_result->data[i] && frame_result->linesize[i] > 0) {
                sz = sizeof(uint8_t) * frame_result->linesize[i];
                if (frame_result->height > 0 && frame_result->nb_samples <= 0) {
                    sz *= frame_result->height;
                }
                (*data)[i] = (uint8_t *) malloc(sz);
                if ((*data)[i]) {
                    memcpy((*data)[i], frame_result->data[i], sz);
                } else {
                    LOGW("malloc [uint8_t *cb_data->data[i]] is NULL!\n");
                }
            }
        }
    } else {
        LOGW("malloc [uint8_t **cb_data->data] is NULL!\n");
    }

    *line_size = (int *) malloc(sizeof(int) * AV_NUM_DATA_POINTERS);
    if (*line_size) {
        memset(*line_size, 0, sizeof(int) * AV_NUM_DATA_POINTERS);
        for (int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
            (*line_size)[i] = frame_result->linesize[i];
        }
    } else {
        LOGW("malloc [int *cb_data->line_size] is NULL!\n");
    }
}

void free_from_frame(uint8_t **data, int *line_size) {
    if (data) {
        for (int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
            if (data[i]) {
                free(data[i]);
            }
        }
        free(data);
    }
    if (line_size) {
        free(line_size);
    }
}

void *play_video_frame(void *data) {
    LOGW("begin play_video_frame!\n");
    FFmpegClient *client = (FFmpegClient *) data;
    if (!client) {
        LOGW("client == NULL!\n");
        return NULL;
    }

    while (true) {
        if (client->stop_video_play && john_synchronized_queue_is_empty(client->video_frame_queue)) {
            break;
        }
        VideoCallbackData *cb_data = (VideoCallbackData *)
                john_synchronized_queue_dequeue(client->video_frame_queue, 1000);
        if (cb_data) {
            if (client->video_out && client->video_out->callback) {
                client->video_out->callback(cb_data->data, cb_data->line_size, cb_data->pixel_precision,
                                            cb_data->width, cb_data->height, cb_data->pts_millis, cb_data->user_tag);
            }
            free_from_frame(cb_data->data, cb_data->line_size);
            free(cb_data);
        }
    }

    LOGW("end play_video_frame!\n");
    return NULL;
}

void *play_audio_frame(void *data) {
    LOGW("begin play_audio_frame!\n");
    FFmpegClient *client = (FFmpegClient *) data;
    if (!client) {
        LOGW("client == NULL!\n");
        return NULL;
    }

    while (true) {
        if (client->stop_audio_play && john_synchronized_queue_is_empty(client->audio_frame_queue)) {
            break;
        }
        AudioCallbackData *cb_data = (AudioCallbackData *)
                john_synchronized_queue_dequeue(client->audio_frame_queue, 1000);
        if (cb_data) {
            if (client->audio_out && client->audio_out->callback) {
                client->audio_out->callback(cb_data->data, cb_data->line_size, cb_data->sample_precision,
                                            cb_data->channels, cb_data->sample_rate,
                                            cb_data->pts_millis, cb_data->user_tag);
            }
            free_from_frame(cb_data->data, cb_data->line_size);
            free(cb_data);
        }
    }

    LOGW("end play_audio_frame!\n");
    return NULL;
}

void *decode_video_frame(void *data) {
    LOGW("begin decode_video_frame!\n");
    FFmpegClient *client = (FFmpegClient *) data;
    if (!client) {
        LOGW("client == NULL!\n");
        return NULL;
    }

    if (client->video_out && client->video_out->callback) {
        if (pthread_create(&client->video_play_thread, NULL, play_video_frame, client)) {
            LOGW("pthread_create [video_play_thread] failed!\n");
            return NULL;
        }
        uint32_t play_queue_size = client->video_out->play_queue_size > 0 ?
                                   client->video_out->play_queue_size : QUEUE_SIZE;
        client->video_frame_queue = john_synchronized_queue_create(play_queue_size, true, NULL);
        if (!client->video_frame_queue) {
            LOGW("john_synchronized_queue_create [video_frame_queue] failed!\n");
            return NULL;
        }
        LOGW("video_frame_queue [%p]\n", client->video_frame_queue);
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
    while (true) {
        if (client->stop_decode && john_synchronized_queue_is_empty(client->video_packet_queue)) {
            break;
        }

        AVPacket *packet = (AVPacket *) john_synchronized_queue_dequeue(client->video_packet_queue, 1000);
        if (!packet) {
            continue;
        }

        error_happened = false;

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

        while ((result_code = avcodec_receive_frame(client->video_codec_context, frame_decoded)) >= 0) {
            if ((result_code = sws_scale(client->sws_context, (const uint8_t *const *)frame_decoded->data,
                                         frame_decoded->linesize, 0, client->video_codec_context->height,
                                         frame_result->data, frame_result->linesize)) < 0) {
                char error_buffer[ERROR_BUFFER_SIZE];
                av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
                LOGW("sws_scale failed with %s!\n", error_buffer);
                goto end_frame;
            }
            if (client->video_out && client->video_out->callback) {
                VideoCallbackData *cb_data = (VideoCallbackData *) malloc(sizeof(VideoCallbackData));
                if (!cb_data) {
                    LOGW("malloc [VideoCallbackData *cb_data] is NULL!\n");
                    goto end_frame;
                }
                memset(cb_data, 0, sizeof(VideoCallbackData));
                alloc_fill_from_frame(&cb_data->data, &cb_data->line_size, frame_result);
                cb_data->pixel_precision = (uint32_t) av_get_bits_per_pixel(av_pix_fmt_desc_get(pixel_format));
                cb_data->width = (uint32_t) width;
                cb_data->height = (uint32_t) height;
                cb_data->pts_millis = get_pts_time_base(client, frame_decoded, client->video_stream_index);
                cb_data->user_tag = client->video_out->user_tag;
                AudioCallbackData *old_cb_data;
                if (!john_synchronized_queue_enqueue(client->video_frame_queue, cb_data,
                                                     (void **) &old_cb_data, -1)) {
                    LOGW("john_synchronized_queue_enqueue [video_frame_queue] failed!\n");
                    goto end_frame;
                }
                if (old_cb_data) {
                    free_from_frame(cb_data->data, cb_data->line_size);
                    free(old_cb_data);
                }
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
        av_packet_free(&packet);
        if (error_happened) {
            break;
        }
    }
    av_frame_free(&frame_decoded);
    LOGW("break loop of decode_video_frame!\n");

    if (client->video_out && client->video_out->callback) {
        client->stop_video_play = true;
        pthread_join(client->video_play_thread, NULL);
        john_synchronized_queue_destroy(client->video_frame_queue);
        LOGW("video play thread quit!\n");
    }

    LOGW("end decode_video_frame!\n");
    return NULL;
}

void *decode_audio_frame(void *data) {
    LOGW("begin decode_audio_frame!\n");
    FFmpegClient *client = (FFmpegClient *) data;
    if (!client) {
        LOGW("client == NULL!\n");
        return NULL;
    }

    if (client->audio_out && client->audio_out->callback) {
        if (pthread_create(&client->audio_play_thread, NULL, play_audio_frame, client)) {
            LOGW("pthread_create [audio_play_thread] failed!\n");
            return NULL;
        }
        uint32_t play_queue_size = client->audio_out->play_queue_size > 0 ?
                                   client->audio_out->play_queue_size : QUEUE_SIZE;
        client->audio_frame_queue = john_synchronized_queue_create(play_queue_size, true, NULL);
        if (!client->audio_frame_queue) {
            LOGW("john_synchronized_queue_create [audio_frame_queue] failed!\n");
            return NULL;
        }
        LOGW("audio_frame_queue [%p]\n", client->audio_frame_queue);
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
    int sample_precision = av_get_bytes_per_sample(sample_format);

    AVFrame *frame_decoded = av_frame_alloc();
    if (!frame_decoded) {
        LOGW("av_frame_alloc failed!\n");
        return NULL;
    }

    int result_code;
    while (true) {
        if (client->stop_decode && john_synchronized_queue_is_empty(client->audio_packet_queue)) {
            break;
        }

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

        while ((result_code = avcodec_receive_frame(client->audio_codec_context, frame_decoded)) >= 0) {
            /* int out_samples1 = (int) av_rescale_rnd(swr_get_delay(client->swr_context, frame_decoded->sample_rate)
                                                    + frame_decoded->nb_samples, frame_result->sample_rate,
                                                    frame_decoded->sample_rate, AV_ROUND_UP);
            int out_samples2 = swr_get_out_samples(client->swr_context, frame_decoded->nb_samples);
            int out_samples3 = (int) (swr_get_delay(client->swr_context, frame_result->sample_rate)
                                      + frame_decoded->nb_samples
                                        * (int64_t) frame_result->sample_rate / frame_decoded->sample_rate
                                      + 3);
            frame_result->nb_samples = out_samples2 > out_samples1 ? out_samples2 : out_samples1;
            if (out_samples3 > frame_result->nb_samples) {
                frame_result->nb_samples = out_samples3;
            } */

            if ((result_code = swr_convert_frame(client->swr_context, frame_result, frame_decoded)) < 0) {
                char error_buffer[ERROR_BUFFER_SIZE];
                av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
                LOGW("swr_convert failed with %s!\n", error_buffer);
                goto end_frame;
            }

            /* I think 'swr_convert_frame' has bug, the value of 'frame_result->linesize[0]' is incorrect! */
            frame_result->linesize[0] = nb_channels * frame_result->nb_samples * sample_precision;

            if (client->audio_out && client->audio_out->callback) {
                AudioCallbackData *cb_data = (AudioCallbackData *) malloc(sizeof(AudioCallbackData));
                if (!cb_data) {
                    LOGW("malloc [AudioCallbackData *cb_data] is NULL!\n");
                    goto end_frame;
                }
                memset(cb_data, 0, sizeof(AudioCallbackData));
                alloc_fill_from_frame(&cb_data->data, &cb_data->line_size, frame_result);
                cb_data->sample_precision = (uint32_t) sample_precision;
                cb_data->channels = (uint32_t) nb_channels;
                cb_data->sample_rate = (uint32_t) sample_rate;
                cb_data->pts_millis = get_pts_time_base(client, frame_decoded, client->audio_stream_index);
                cb_data->user_tag = client->audio_out->user_tag;
                AudioCallbackData *old_cb_data;
                if (!john_synchronized_queue_enqueue(client->audio_frame_queue, cb_data,
                                                     (void **) &old_cb_data, -1)) {
                    LOGW("john_synchronized_queue_enqueue [audio_frame_queue] failed!\n");
                    goto end_frame;
                }
                if (old_cb_data) {
                    free_from_frame(cb_data->data, cb_data->line_size);
                    free(old_cb_data);
                }
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
        av_packet_free(&packet);
    }
    av_frame_free(&frame_decoded);
    LOGW("break loop of decode_audio_frame!\n");

    if (client->audio_out && client->audio_out->callback) {
        client->stop_audio_play = true;
        pthread_join(client->audio_play_thread, NULL);
        john_synchronized_queue_destroy(client->audio_frame_queue);
        LOGW("audio play thread quit!\n");
    }

    LOGW("end decode_audio_frame!\n");
    return NULL;
}

void loop_read_frame(FFmpegClient *client, int64_t start_play_millis) {
    LOGW("begin loop_read_frame!\n");
    if (!client) {
        LOGW("client == NULL!\n");
        return;
    }

    if (client->video_stream_index >= 0) {
        if (pthread_create(&client->video_decode_thread, NULL, decode_video_frame, client)) {
            LOGW("pthread_create [video_decode_thread] failed!\n");
            return;
        }
        client->video_packet_queue = john_synchronized_queue_create(QUEUE_SIZE, true, NULL);
        if (!client->video_packet_queue) {
            LOGW("john_synchronized_queue_create [video_packet_queue] failed!\n");
            return;
        }
        LOGW("video_packet_queue [%p]\n", client->video_packet_queue);

        if (start_play_millis > 0) {
            AVRational time_base = client->format_context->streams[client->video_stream_index]->time_base;
            int64_t timestamp = av_rescale_q(start_play_millis * 1000, AV_TIME_BASE_Q, time_base);
            int result_code;
            if ((result_code = av_seek_frame(client->format_context, client->video_stream_index,
                                             timestamp, AVSEEK_FLAG_BACKWARD)) < 0) {
                char error_buffer[ERROR_BUFFER_SIZE];
                av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
                LOGW("av_seek_frame(video_stream_index) failed with %s!\n", error_buffer);
            } else {
                avcodec_flush_buffers(client->video_codec_context);
            }
        }
    }

    if (client->audio_stream_index >= 0) {
        if (pthread_create(&client->audio_decode_thread, NULL, decode_audio_frame, client)) {
            LOGW("pthread_create [audio_decode_thread] failed!\n");
            return;
        }
        client->audio_packet_queue = john_synchronized_queue_create(QUEUE_SIZE, true, NULL);
        if (!client->audio_packet_queue) {
            LOGW("john_synchronized_queue_create [audio_packet_queue] failed!\n");
            return;
        }
        LOGW("audio_packet_queue [%p]\n", client->audio_packet_queue);

        if (start_play_millis > 0) {
            AVRational time_base = client->format_context->streams[client->audio_stream_index]->time_base;
            int64_t timestamp = av_rescale_q(start_play_millis * 1000, AV_TIME_BASE_Q, time_base);
            int result_code;
            if ((result_code = av_seek_frame(client->format_context, client->audio_stream_index,
                                             timestamp, AVSEEK_FLAG_BACKWARD)) < 0) {
                char error_buffer[ERROR_BUFFER_SIZE];
                av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
                LOGW("av_seek_frame(audio_stream_index) failed with %s!\n", error_buffer);
            } else {
                avcodec_flush_buffers(client->audio_codec_context);
            }
        }
    }

    while (true) {
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
                LOGW("john_synchronized_queue_enqueue [video_packet_queue] failed!\n");
                break;
            }

            if (old_packet) {
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
                LOGW("john_synchronized_queue_enqueue [audio_packet_queue] failed!\n");
                break;
            }

            if (old_packet) {
                av_packet_free(&old_packet);
            }
#endif /* ONLY_BACKUP */
        }
    }

    pthread_mutex_t loop_mutex;
    pthread_cond_t sleep_cond;
    pthread_mutex_init(&loop_mutex, NULL);
    pthread_cond_init(&sleep_cond, NULL);
    thread_sleep(&loop_mutex, &sleep_cond, 1000);
    pthread_cond_destroy(&sleep_cond);
    pthread_mutex_destroy(&loop_mutex);

    client->stop_decode = true;
    if (client->video_stream_index >= 0) {
        pthread_join(client->video_decode_thread, NULL);
        john_synchronized_queue_destroy(client->video_packet_queue);
        LOGW("video decode thread quit!\n");
    }
    if (client->audio_stream_index >= 0) {
        pthread_join(client->audio_decode_thread, NULL);
        john_synchronized_queue_destroy(client->audio_packet_queue);
        LOGW("audio decode thread quit!\n");
    }
    LOGW("end loop_read_frame!\n");
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

#include <SDL2/SDL.h>
#include <signal.h>

typedef struct SdlCtx {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_AudioDeviceID audio_device_id;
    int64_t video_pts_millis;
    int64_t audio_pts_millis;
    pthread_mutex_t video_thread_mutex;
    pthread_mutex_t audio_thread_mutex;
    pthread_cond_t video_thread_sleep_cond;
    pthread_cond_t audio_thread_sleep_cond;
} SdlCtx;

static SdlCtx *SdlCtx_create() {
    SdlCtx *sdl_ctx = (SdlCtx *) malloc(sizeof(SdlCtx));
    if (!sdl_ctx) {
        LOGW( "malloc SdlCtx *sdl_ctx failed!\n");
        return NULL;
    }
    memset(sdl_ctx, 0, sizeof(SdlCtx));
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        LOGW( "Could not initialize SDL - %s\n", SDL_GetError());
        free(sdl_ctx);
        return NULL;
    }
    return sdl_ctx;
}

static void SdlCtx_destroy(SdlCtx *sdl_ctx) {
    if (sdl_ctx) {
        SDL_Quit();
        free(sdl_ctx);
    }
}

static int SdlCtx_createVideo(SdlCtx *sdl_ctx, int width, int height) {
    if (!sdl_ctx) {
        LOGW("sdl_ctx is NULL!\n");
        return -1;
    }
    sdl_ctx->window = SDL_CreateWindow("SDL-Play",
                                         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                         width, height, SDL_WINDOW_SHOWN);
    if (!sdl_ctx->window) {
        LOGW("Call SDL_CreateWindow failed with %s!\n", SDL_GetError());
        return -1;
    }
    sdl_ctx->renderer = SDL_CreateRenderer(sdl_ctx->window, -1,
                                             SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdl_ctx->renderer) {
        LOGW("Call SDL_CreateRenderer failed with %s!\n", SDL_GetError());
        return -1;
    }
    sdl_ctx->texture = SDL_CreateTexture(sdl_ctx->renderer, SDL_PIXELFORMAT_BGR24,
                                           SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!sdl_ctx->texture) {
        LOGW("Call SDL_CreateTexture failed with %s!\n", SDL_GetError());
        return -1;
    }

    pthread_mutex_init(&sdl_ctx->video_thread_mutex, NULL);
    pthread_cond_init(&sdl_ctx->video_thread_sleep_cond, NULL);
    return 0;
}

static void SdlCtx_destroyVideo(SdlCtx *sdl_ctx) {
    if (sdl_ctx) {
        if (sdl_ctx->texture) {
            SDL_DestroyTexture(sdl_ctx->texture);
        }
        if (sdl_ctx->renderer) {
            SDL_DestroyRenderer(sdl_ctx->renderer);
        }
        if (sdl_ctx->window) {
            SDL_DestroyWindow(sdl_ctx->window);
        }

        pthread_cond_destroy(&sdl_ctx->video_thread_sleep_cond);
        pthread_mutex_destroy(&sdl_ctx->video_thread_mutex);
    }
}

static int SdlCtx_createAudio(SdlCtx *sdl_ctx, int sample_rate, int frame_sample) {
    if (!sdl_ctx) {
        LOGW("sdl_ctx is NULL!\n");
        return -1;
    }
    SDL_AudioSpec wanted_spec, obtained_spec;
    SDL_zero(wanted_spec);
    wanted_spec.freq = sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = 1;
    wanted_spec.silence = 0;
    wanted_spec.samples = (Uint16) frame_sample;
    wanted_spec.callback = NULL;
    sdl_ctx->audio_device_id = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &obtained_spec,
                                                   SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (sdl_ctx->audio_device_id == 0) {
        LOGW("Call SDL_OpenAudioDevice failed with %s!\n", SDL_GetError());
        return -1;
    }
    if (obtained_spec.format != wanted_spec.format) { /* we let this one thing change. */
        LOGW("Using closest audio format which sound card supported instead!\n");
    }

    SDL_PauseAudioDevice(sdl_ctx->audio_device_id, 0); /* start audio playing. */

    pthread_mutex_init(&sdl_ctx->audio_thread_mutex, NULL);
    pthread_cond_init(&sdl_ctx->audio_thread_sleep_cond, NULL);
    return 0;
}

static void SdlCtx_destroyAudio(SdlCtx *sdl_ctx) {
    if (sdl_ctx) {
        /* wait audio play finish */
        while (SDL_GetQueuedAudioSize(sdl_ctx->audio_device_id) > 0);

        SDL_PauseAudioDevice(sdl_ctx->audio_device_id, 1); /* stop audio playing. */
        SDL_CloseAudioDevice(sdl_ctx->audio_device_id);

        pthread_cond_destroy(&sdl_ctx->audio_thread_sleep_cond);
        pthread_mutex_destroy(&sdl_ctx->audio_thread_mutex);
    }
}

static void video_callback(uint8_t **data, int *line_size, uint32_t pixel_precision,
                           uint32_t width, uint32_t height, int64_t pts_millis, void *user_tag) {
    static bool has_raise = false;
    SdlCtx *sdl_ctx = (SdlCtx *) user_tag;
    if (sdl_ctx) {
        if (!has_raise) {
            SDL_RaiseWindow(sdl_ctx->window);
            has_raise = true;
        }
        SDL_UpdateTexture(sdl_ctx->texture, NULL, data[0], line_size[0]);
        SDL_RenderClear(sdl_ctx->renderer);
        SDL_RenderCopy(sdl_ctx->renderer, sdl_ctx->texture, NULL, NULL);
        SDL_RenderPresent(sdl_ctx->renderer);

        Uint32 delay;
        if (pts_millis >= 0) {
            if (llabs(pts_millis - sdl_ctx->video_pts_millis) > SEEK_THRESHOLD) {
                LOGW("detach video seek: %lld, %lld\n", pts_millis, sdl_ctx->audio_pts_millis);
                sdl_ctx->video_pts_millis = pts_millis;
                return;
            }
            if (pts_millis < sdl_ctx->video_pts_millis) {
                LOGW("pts_millis < sdl_ctx->video_pts_millis: %lld, %lld\n", pts_millis, sdl_ctx->video_pts_millis);
                sdl_ctx->video_pts_millis = pts_millis;
                return;
            }
            delay = (Uint32) (pts_millis - sdl_ctx->video_pts_millis);
        } else {
            delay = 40;
        }
        if (sdl_ctx->audio_pts_millis - sdl_ctx->video_pts_millis < 0) {
            thread_sleep(&sdl_ctx->video_thread_mutex, &sdl_ctx->video_thread_sleep_cond, delay * 2);
        } else if (sdl_ctx->audio_pts_millis - sdl_ctx->video_pts_millis < delay) {
            thread_sleep(&sdl_ctx->video_thread_mutex, &sdl_ctx->video_thread_sleep_cond, delay);
        }
        sdl_ctx->video_pts_millis += delay;
    }
}

static void audio_callback(uint8_t **data, int *line_size, uint32_t sample_precision,
                           uint32_t channels, uint32_t sample_rate, int64_t pts_millis, void *user_tag) {
    SdlCtx *sdl_ctx = (SdlCtx *) user_tag;
    if (sdl_ctx) {
        SDL_QueueAudio(sdl_ctx->audio_device_id, data[0], (Uint32) line_size[0]);

        Uint32 delay;
        if (pts_millis >= 0) {
            if (llabs(pts_millis - sdl_ctx->audio_pts_millis) > SEEK_THRESHOLD) {
                LOGW("detach audio seek: %lld, %lld\n", pts_millis, sdl_ctx->audio_pts_millis);
                sdl_ctx->audio_pts_millis = pts_millis;
                return;
            }
            if (pts_millis < sdl_ctx->audio_pts_millis) {
                LOGW("pts_millis < sdl_ctx->audio_pts_millis: %lld, %lld\n", pts_millis, sdl_ctx->audio_pts_millis);
                sdl_ctx->audio_pts_millis = pts_millis;
                return;
            }
            delay = (Uint32) (pts_millis - sdl_ctx->audio_pts_millis);
        } else {
            delay = 25;
        }
        thread_sleep(&sdl_ctx->audio_thread_mutex, &sdl_ctx->audio_thread_sleep_cond, delay);
        sdl_ctx->audio_pts_millis += delay;
    }
}

static void handle_signal(int sig) {
    if (sig == SIGQUIT || sig == SIGINT) {
        exit(0);
    }
}

int main(int argc, char **argv) {
    const char *url;
    if (argc > 1) {
        url = argv[1];
    } else {
        url = BASE_PATH"/data/test.mp4";
    }

    int64_t start_play_millis;
    if (argc > 2) {
        if (av_parse_time(&start_play_millis, argv[2], 1) >= 0) {
            start_play_millis /= 1000;
        } else {
            LOGW("Call av_parse_time() failed!\n");
            start_play_millis = 0;
        }
    } else {
        start_play_millis = 0;
    }

    struct sigaction sigact;
    sigact.sa_handler = handle_signal;
    sigact.sa_flags = 0;
    sigaction(SIGQUIT, &sigact, NULL);
    sigaction(SIGINT, &sigact, NULL);

    trace_malloc_free_create();

    SdlCtx *sdl_ctx = SdlCtx_create();
    if (!sdl_ctx) {
        LOGW("Call SdlCtx_create() failed!\n");
        return -1;
    }

    VideoOutRule videoOutRule;
    videoOutRule.width = 800;
    videoOutRule.height = 0;
    videoOutRule.pixel_format = AV_PIX_FMT_BGR24;
    videoOutRule.callback = video_callback;
    videoOutRule.user_tag = sdl_ctx;
    videoOutRule.play_queue_size = 0;
    videoOutRule.has_video = false;

    AudioOutRule audioOutRule;
    audioOutRule.sample_format = AV_SAMPLE_FMT_S16;
    audioOutRule.sample_rate = 16000;
    audioOutRule.channel_layout = AV_CH_LAYOUT_MONO;
    audioOutRule.callback = audio_callback;
    audioOutRule.user_tag = sdl_ctx;
    audioOutRule.play_queue_size = 0;
    audioOutRule.has_audio = false;

    FFmpegClient *client = open_media(url, &videoOutRule, &audioOutRule);
    if (client) {
        if (videoOutRule.has_video) {
            if (SdlCtx_createVideo(sdl_ctx, videoOutRule.width, videoOutRule.height) < 0) {
                LOGW("Call SdlCtx_createVideo() failed!\n");
                return -1;
            }
        }
        if (audioOutRule.has_audio) {
            if (SdlCtx_createAudio(sdl_ctx, audioOutRule.sample_rate, 1024) < 0) {
                LOGW("Call SdlCtx_createAudio() failed!\n");
                return -1;
            }
        }

        loop_read_frame(client, start_play_millis);
        close_media(client);

        if (videoOutRule.has_video) {
            SdlCtx_destroyVideo(sdl_ctx);
        }
        if (audioOutRule.has_audio) {
            SdlCtx_destroyAudio(sdl_ctx);
        }
    }

    SdlCtx_destroy(sdl_ctx);

    trace_malloc_free_destroy();

    return 0;
}