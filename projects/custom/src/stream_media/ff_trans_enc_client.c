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
 * @version 2018-01-18
 */
#include <stdint.h>
#include <stdbool.h>
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/parseutils.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include "common.h"

#define ERROR_BUFFER_SIZE 512
#define FILE_PATH_BUFFER_SIZE 512

typedef struct FFmpegEndpoint {
    const char *url;
    AVFormatContext *format_context;
    AVCodecContext *video_codec_context;
    AVCodecContext *audio_codec_context;
    int video_stream_index;
    int audio_stream_index;
} FFmpegEndpoint;

typedef void (*ReceiveFrameFunc)(FFmpegEndpoint *input, FFmpegEndpoint *output, AVFrame *frame_decoded);

FFmpegEndpoint *fftec_open_input(const char *url);
void fftec_close_input(FFmpegEndpoint *input);
void fftec_loop_decode_transfer(FFmpegEndpoint *input, FFmpegEndpoint *output,
                                ReceiveFrameFunc receive_video_frame,
                                ReceiveFrameFunc receive_audio_frame);

FFmpegEndpoint *fftec_open_output(const char *url, FFmpegEndpoint *input);
void fftec_close_output(FFmpegEndpoint *output);
void fftec_encode_video_frame(FFmpegEndpoint *input, FFmpegEndpoint *output, AVFrame *frame_decoded);
void fftec_encode_audio_frame(FFmpegEndpoint *input, FFmpegEndpoint *output, AVFrame *frame_decoded);

void fftec_record_video_frame(FFmpegEndpoint *input, FFmpegEndpoint *output, AVFrame *frame_decoded);
void fftec_record_audio_frame(FFmpegEndpoint *input, FFmpegEndpoint *output, AVFrame *frame_decoded);

FFmpegEndpoint *fftec_open_input(const char *url) {
    LOGW("fftec_open_input!\n");
    if (!url) {
        LOGW("url == NULL!\n");
        return NULL;
    }

    FFmpegEndpoint *input = (FFmpegEndpoint *) malloc(sizeof(FFmpegEndpoint));
    if (!input) {
        LOGW("malloc FFmpegEndpoint *input failed!\n");
        return NULL;
    }
    memset(input, 0, sizeof(FFmpegEndpoint));
    input->video_stream_index = input->audio_stream_index = -1;
    input->url = url;

    av_register_all();
    avcodec_register_all();

    int result_code;

    if ((result_code = avformat_open_input(&input->format_context, url, NULL, NULL))) {
        char error_buffer[ERROR_BUFFER_SIZE];
        av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
        LOGW("avformat_open_input failed with %s!\n", error_buffer);
        free(input);
        return NULL;
    }

    if (avformat_find_stream_info(input->format_context, NULL) < 0) {
        LOGW("avformat_find_stream_info failed!\n");
        fftec_close_input(input);
        return NULL;
    }

    int index;
    if ((index = av_find_best_stream(input->format_context, AVMEDIA_TYPE_VIDEO,
                                     -1, -1, NULL, 0)) < 0) {
        LOGW("not found any video stream!\n");
    } else {
        av_dump_format(input->format_context, index, url, false);
    }
    input->video_stream_index = index;

    if ((index = av_find_best_stream(input->format_context, AVMEDIA_TYPE_AUDIO,
                                     -1, -1, NULL, 0)) < 0) {
        LOGW("not found any audio stream!\n");
    } else {
        av_dump_format(input->format_context, index, url, false);
    }
    input->audio_stream_index = index;

    if (input->video_stream_index < 0 && input->audio_stream_index < 0) {
        fftec_close_input(input);
        return NULL;
    }

    if (input->video_stream_index >= 0) {
        AVStream *video_stream = input->format_context->streams[input->video_stream_index];

        AVCodec *video_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
        if (!video_codec) {
            LOGW("avcodec_find_decoder [video_codec] failed!\n");
            fftec_close_input(input);
            return NULL;
        }

        input->video_codec_context = avcodec_alloc_context3(video_codec);
        if (!input->video_codec_context) {
            LOGW("avcodec_alloc_context3 [video_codec_context] failed!\n");
            fftec_close_input(input);
            return NULL;
        }

        if (avcodec_parameters_to_context(input->video_codec_context,
                                          video_stream->codecpar) < 0) {
            LOGW("avcodec_parameters_to_context [video_codec_context/video_stream] failed!\n");
            fftec_close_input(input);
            return NULL;
        }

        av_codec_set_pkt_timebase(input->video_codec_context, video_stream->time_base);

        if (avcodec_open2(input->video_codec_context, video_codec, NULL)) {
            LOGW("avcodec_open2 [video_codec_context/video_stream] failed!\n");
            fftec_close_input(input);
            return NULL;
        }
    }

    if (input->audio_stream_index >= 0) {
        AVStream *audio_stream = input->format_context->streams[input->audio_stream_index];

        AVCodec *audio_codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
        if (!audio_codec) {
            LOGW("avcodec_find_decoder [audio_codec] failed!\n");
            fftec_close_input(input);
            return NULL;
        }

        input->audio_codec_context = avcodec_alloc_context3(audio_codec);
        if (!input->audio_codec_context) {
            LOGW("avcodec_alloc_context3 [audio_codec_context] failed!\n");
            fftec_close_input(input);
            return NULL;
        }

        if (avcodec_parameters_to_context(input->audio_codec_context,
                                          audio_stream->codecpar) < 0) {
            LOGW("avcodec_parameters_to_context [audio_codec_context/audio_stream] failed!\n");
            fftec_close_input(input);
            return NULL;
        }

        av_codec_set_pkt_timebase(input->audio_codec_context, audio_stream->time_base);

        if (avcodec_open2(input->audio_codec_context, audio_codec, NULL)) {
            LOGW("avcodec_open2 [audio_codec_context/audio_stream] failed!\n");
            fftec_close_input(input);
            return NULL;
        }

        if (input->audio_codec_context->channel_layout == 0
            && input->audio_codec_context->channels > 0) {
            input->audio_codec_context->channel_layout =
                    (uint64_t) av_get_default_channel_layout(input->audio_codec_context->channels);
        }
    }

    return input;
}

void fftec_close_input(FFmpegEndpoint *input) {
    LOGW("fftec_close_input!\n");
    if (input) {
        if (input->video_stream_index >= 0) {
            avcodec_close(input->video_codec_context);
            avcodec_free_context(&input->video_codec_context);
        }

        if (input->audio_stream_index >= 0) {
            avcodec_close(input->audio_codec_context);
            avcodec_free_context(&input->audio_codec_context);
        }

        avformat_close_input(&input->format_context);
        avformat_free_context(input->format_context);
        free(input);
    }
}

void fftec_loop_decode_transfer(FFmpegEndpoint *input, FFmpegEndpoint *output,
                                ReceiveFrameFunc receive_video_frame,
                                ReceiveFrameFunc receive_audio_frame) {
    LOGW("begin fftec_loop_decode_transfer!\n");
    if (!input) {
        LOGW("Error: FFmpegEndpoint *input == NULL!\n");
        return;
    }

    while (true) {
        AVPacket *packet = av_packet_alloc();
        if (!packet) {
            LOGW("av_packet_alloc failed!\n");
            break;
        }
        av_init_packet(packet);

        int result_code;

        if ((result_code = av_read_frame(input->format_context, packet)) < 0) {
            char error_buffer[ERROR_BUFFER_SIZE];
            av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
            LOGW("av_read_frame result with %s\n", error_buffer);
            av_packet_free(&packet);
            break;
        }

        AVFrame *frame_decoded = av_frame_alloc();
        if (!frame_decoded) {
            LOGW("av_frame_alloc failed!\n");
            av_packet_free(&packet);
            break;
        }

        if (packet->stream_index == input->video_stream_index) {
            if ((result_code = avcodec_send_packet(input->video_codec_context, packet))) {
                char error_buffer[ERROR_BUFFER_SIZE];
                av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
                LOGW("avcodec_send_packet [video] failed with %s!\n", error_buffer);
                av_frame_free(&frame_decoded);
                av_packet_free(&packet);
                break;
            }
            while ((result_code = avcodec_receive_frame(input->video_codec_context, frame_decoded)) >= 0) {
                if (receive_video_frame) {
                    receive_video_frame(input, output, frame_decoded);
                }
            }
            if (result_code != -EAGAIN) {
                char error_buffer[ERROR_BUFFER_SIZE];
                av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
                LOGW("avcodec_receive_frame [video] result with %s\n", error_buffer);
                av_frame_free(&frame_decoded);
                av_packet_free(&packet);
                break;
            }
        } else if (packet->stream_index == input->audio_stream_index) {
            if ((result_code = avcodec_send_packet(input->audio_codec_context, packet))) {
                char error_buffer[ERROR_BUFFER_SIZE];
                av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
                LOGW("avcodec_send_packet [audio] failed with %s!\n", error_buffer);
                av_frame_free(&frame_decoded);
                av_packet_free(&packet);
                break;
            }
            while ((result_code = avcodec_receive_frame(input->audio_codec_context, frame_decoded)) >= 0) {
                if (receive_audio_frame) {
                    receive_audio_frame(input, output, frame_decoded);
                }
            }
            if (result_code != -EAGAIN) {
                char error_buffer[ERROR_BUFFER_SIZE];
                av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
                LOGW("avcodec_receive_frame [audio] result with %s\n", error_buffer);
                av_frame_free(&frame_decoded);
                av_packet_free(&packet);
                break;
            }
        }

        av_frame_free(&frame_decoded);
        av_packet_free(&packet);
    }

    LOGW("end fftec_loop_decode_transfer!\n");
}

FFmpegEndpoint *fftec_open_output(const char *url, FFmpegEndpoint *input) {
    LOGW("fftec_open_output!\n");
    if (!url || !input) {
        LOGW("url == NULL || input == NULL!\n");
        return NULL;
    }

    FFmpegEndpoint *output = (FFmpegEndpoint *) malloc(sizeof(FFmpegEndpoint));
    if (!output) {
        LOGW("malloc FFmpegEndpoint *output failed!\n");
        return NULL;
    }
    memset(output, 0, sizeof(FFmpegEndpoint));
    output->video_stream_index = input->audio_stream_index = -1;
    output->url = url;

    av_register_all();
    avcodec_register_all();

    int result_code;

    if ((result_code = avformat_alloc_output_context2(&output->format_context, NULL,
                                       input->format_context->iformat->name, output->url))) {
        char error_buffer[ERROR_BUFFER_SIZE];
        av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
        LOGW("avformat_alloc_output_context2 failed with %s!\n", error_buffer);
        free(output);
        return NULL;
    }

    output->video_stream_index = input->video_stream_index;
    output->audio_stream_index = input->audio_stream_index;
    if (output->video_stream_index < 0 && output->audio_stream_index < 0) {
        LOGW("no valid stream index found!\n");
        fftec_close_input(output);
        return NULL;
    }

    if (output->video_stream_index >= 0) {
        output->format_context->streams[output->video_stream_index] = avformat_new_stream(output->format_context, NULL);
        if (!output->format_context->streams[output->video_stream_index]) {
            LOGW("avformat_new_stream [video] failed!\n");
            fftec_close_output(output);
            return NULL;
        }

        AVCodec *video_codec = avcodec_find_encoder(input->video_codec_context->codec_id);
        if (!video_codec) {
            LOGW("avcodec_find_encoder [video] failed!\n");
            fftec_close_output(output);
            return NULL;
        }

        output->video_codec_context = avcodec_alloc_context3(video_codec);
        if (!output->video_codec_context) {
            LOGW("avcodec_alloc_context3 [video_codec_context] failed!\n");
            fftec_close_input(output);
            return NULL;
        }

        if ((result_code = avcodec_parameters_to_context(output->video_codec_context,
                                          input->format_context->streams[input->video_stream_index]->codecpar)) < 0) {
            char error_buffer[ERROR_BUFFER_SIZE];
            av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
            LOGW("avcodec_parameters_to_context [video_codec_context/video_stream] failed with %s!\n", error_buffer);
            fftec_close_output(output);
            return NULL;
        }

        av_codec_set_pkt_timebase(output->video_codec_context,
                                  input->format_context->streams[input->video_stream_index]->time_base);

        if ((result_code = avcodec_open2(output->video_codec_context, video_codec, NULL))) {
            char error_buffer[ERROR_BUFFER_SIZE];
            av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
            LOGW("avcodec_open2 [video_codec_context/video_stream] failed with %s!\n", error_buffer);
            fftec_close_input(output);
            return NULL;
        }

        if (output->format_context->oformat->flags & AVFMT_GLOBALHEADER) {
            output->video_codec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }
    }

    if (output->audio_stream_index >= 0) {
        output->format_context->streams[output->audio_stream_index] = avformat_new_stream(output->format_context, NULL);
        if (!output->format_context->streams[output->audio_stream_index]) {
            LOGW("avformat_new_stream [audio] failed!\n");
            fftec_close_output(output);
            return NULL;
        }

        AVCodec *audio_codec = avcodec_find_encoder(input->audio_codec_context->codec_id);
        if (!audio_codec) {
            LOGW("avcodec_find_encoder [audio] failed!\n");
            fftec_close_output(output);
            return NULL;
        }

        output->audio_codec_context = avcodec_alloc_context3(audio_codec);
        if (!output->audio_codec_context) {
            LOGW("avcodec_alloc_context3 [audio_codec_context] failed!\n");
            fftec_close_input(output);
            return NULL;
        }

        if ((result_code = avcodec_parameters_to_context(output->audio_codec_context,
                                          input->format_context->streams[input->audio_stream_index]->codecpar)) < 0) {
            char error_buffer[ERROR_BUFFER_SIZE];
            av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
            LOGW("avcodec_parameters_to_context [audio_codec_context/audio_stream] failed with %s!\n", error_buffer);
            fftec_close_output(output);
            return NULL;
        }

        av_codec_set_pkt_timebase(output->audio_codec_context,
                                  input->format_context->streams[input->audio_stream_index]->time_base);

        if ((result_code = avcodec_open2(output->audio_codec_context, audio_codec, NULL))) {
            char error_buffer[ERROR_BUFFER_SIZE];
            av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
            LOGW("avcodec_open2 [audio_codec_context/audio_stream] failed with %s!\n", error_buffer);
            fftec_close_input(output);
            return NULL;
        }

        if (output->format_context->oformat->flags & AVFMT_GLOBALHEADER) {
            output->audio_codec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }

        if (output->audio_codec_context->channel_layout == 0
            && output->audio_codec_context->channels > 0) {
            output->audio_codec_context->channel_layout =
                    (uint64_t) av_get_default_channel_layout(output->audio_codec_context->channels);
        }
    }

    av_dump_format(output->format_context, 0, url, true);

    if (!(output->format_context->oformat->flags & AVFMT_NOFILE)) {
        if ((result_code = avio_open(&output->format_context->pb, url, AVIO_FLAG_WRITE)) < 0) {
            char error_buffer[ERROR_BUFFER_SIZE];
            av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
            LOGW("avio_open failed with %s!\n", error_buffer);
            fftec_close_input(output);
            return NULL;
        }
    }

    if ((result_code = avformat_write_header(output->format_context, NULL)) < 0) {
        char error_buffer[ERROR_BUFFER_SIZE];
        av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
        LOGW("avformat_write_header failed with %s!\n", error_buffer);
        fftec_close_input(output);
        return NULL;
    }

    return output;
}

void fftec_close_output(FFmpegEndpoint *output) {
    LOGW("fftec_close_output!\n");
    if (output) {
        if (output->video_stream_index >= 0) {
            avcodec_close(output->video_codec_context);
            avcodec_free_context(&output->video_codec_context);
        }

        if (output->audio_stream_index >= 0) {
            avcodec_close(output->audio_codec_context);
            avcodec_free_context(&output->audio_codec_context);
        }

        avformat_close_input(&output->format_context);
        avformat_free_context(output->format_context);
        free(output);
    }
}

void fftec_encode_video_frame(FFmpegEndpoint *input, FFmpegEndpoint *output, AVFrame *frame_decoded) {
    if (!input || !output || !frame_decoded) {
        LOGW("Error: fftec_encode_video_frame() called with NULL parameter!\n");
        return;
    }

    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        LOGW("av_packet_alloc [video] failed!\n");
        return;
    }
    av_init_packet(packet);
    packet->stream_index = output->video_stream_index;

    int result_code;

    if ((result_code = avcodec_send_frame(output->video_codec_context, frame_decoded))) {
        char error_buffer[ERROR_BUFFER_SIZE];
        av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
        LOGW("avcodec_send_frame [video] failed with %s!\n", error_buffer);
        av_packet_free(&packet);
        return;
    }
    while ((result_code = avcodec_receive_packet(output->video_codec_context, packet)) >= 0) {
        if ((result_code = av_interleaved_write_frame(output->format_context, packet)) < 0) {
            char error_buffer[ERROR_BUFFER_SIZE];
            av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
            LOGW("av_write_frame [video] result with %s\n", error_buffer);
            av_packet_free(&packet);
            return;
        }
    }
    if (result_code != -EAGAIN) {
        char error_buffer[ERROR_BUFFER_SIZE];
        av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
        LOGW("avcodec_receive_packet [video] result with %s\n", error_buffer);
        av_packet_free(&packet);
        return;
    }

    av_packet_free(&packet);
}

void fftec_encode_audio_frame(FFmpegEndpoint *input, FFmpegEndpoint *output, AVFrame *frame_decoded) {
    if (!input || !output || !frame_decoded) {
        LOGW("Error: fftec_encode_audio_frame() called with NULL parameter!\n");
        return;
    }

    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        LOGW("av_packet_alloc [audio] failed!\n");
        return;
    }
    av_init_packet(packet);
    packet->stream_index = output->audio_stream_index;

    int result_code;

    if ((result_code = avcodec_send_frame(output->audio_codec_context, frame_decoded))) {
        char error_buffer[ERROR_BUFFER_SIZE];
        av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
        LOGW("avcodec_send_frame [audio] failed with %s!\n", error_buffer);
        av_packet_free(&packet);
        return;
    }
    while ((result_code = avcodec_receive_packet(output->audio_codec_context, packet)) >= 0) {
        if ((result_code = av_interleaved_write_frame(output->format_context, packet)) < 0) {
            char error_buffer[ERROR_BUFFER_SIZE];
            av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
            LOGW("av_write_frame [audio] result with %s\n", error_buffer);
            av_packet_free(&packet);
            return;
        }
    }
    if (result_code != -EAGAIN) {
        char error_buffer[ERROR_BUFFER_SIZE];
        av_strerror(result_code, error_buffer, ERROR_BUFFER_SIZE);
        LOGW("avcodec_receive_packet [audio] result with %s\n", error_buffer);
        av_packet_free(&packet);
        return;
    }

    av_packet_free(&packet);
}

/*
 * Param FFmpegEndpoint *output not used;
 *
 * Examples:
 * ffplay -pix_fmts
 * ffplay -i raw.yuv -pixel_format yuv420p -video_size 1280x720
 */
void fftec_record_video_frame(FFmpegEndpoint *input, FFmpegEndpoint *output, AVFrame *frame_decoded) {
    if (input && frame_decoded) {
        static FILE *bak;
        if (!bak) {
            char buffer[FILE_PATH_BUFFER_SIZE];
            sprintf(buffer, "%s.%dx%d.%s.yuv", input->url, frame_decoded->width, frame_decoded->height,
                    av_get_pix_fmt_name((enum AVPixelFormat) frame_decoded->format));
            remove(buffer);
            bak = fopen(buffer, "ab");
        }
        if (bak) {
            const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get((enum AVPixelFormat) frame_decoded->format);
            int frame_size = 0;
            for (int i = 0; i < desc->nb_components; ++i) {
                if (i == 0) {
                    frame_size = frame_decoded->linesize[i] * frame_decoded->height;
                } else {
                    frame_size = frame_decoded->linesize[i] * -((-frame_decoded->height)>>desc->log2_chroma_h);
                }
                fwrite(frame_decoded->data[i], (size_t) frame_size, 1, bak);
            }
            fflush(bak);
        }
    }
}

/*
 * Param FFmpegEndpoint *output not used;
 *
 * Examples:
 * ffplay -formats | grep PCM
 * ffplay -i raw.pcm -f s16le -ar 48000 -ac 2
 */
void fftec_record_audio_frame(FFmpegEndpoint *input, FFmpegEndpoint *output, AVFrame *frame_decoded) {
    if (input && frame_decoded) {
        static FILE *bak;
        if (!bak) {
            char buffer[FILE_PATH_BUFFER_SIZE];
            sprintf(buffer, "%s.%d_%d_%s.pcm", input->url, frame_decoded->sample_rate, frame_decoded->channels,
                    av_get_sample_fmt_name((enum AVSampleFormat) frame_decoded->format));
            remove(buffer);
            bak = fopen(buffer, "ab");
        }
        if (bak) {
            if (av_sample_fmt_is_planar((enum AVSampleFormat) frame_decoded->format)) {
                const int nb_channels = frame_decoded->channels;
                const int buffer_size = frame_decoded->linesize[0];
                const int channel_size = buffer_size >> 1;
                const int byte_count = av_get_bytes_per_sample((enum AVSampleFormat) frame_decoded->format);
                uint8_t *buffer = (uint8_t *) malloc((size_t) buffer_size);
                int k = 0;
                for (int i = 0; i < channel_size; i += byte_count) {
                    for (int j = 0; j < nb_channels; ++j) {
                        memcpy(buffer + k, frame_decoded->data[j] + i, byte_count);
                        k += byte_count;
                    }
                }
                fwrite(buffer, (size_t) buffer_size, 1, bak);
                free(buffer);
            } else {
                fwrite(frame_decoded->data[0], (size_t) frame_decoded->linesize[0], 1, bak);
            }
            fflush(bak);
        }
    }
}

#include "base_path.h"

static void test_decode_record() {
    FFmpegEndpoint *input = fftec_open_input(BASE_PATH"/data/test.mp4");
    fftec_loop_decode_transfer(input, NULL, fftec_record_video_frame, fftec_record_audio_frame);
    fftec_close_input(input);
}

int main(int argc, char **argv) {
    FFmpegEndpoint *input = fftec_open_input(BASE_PATH"/data/test.mp4");
    input->format_context->iformat->name = "flv";
    FFmpegEndpoint *output = fftec_open_output(BASE_PATH"/data/test.flv", input);
    fftec_loop_decode_transfer(input, output, fftec_encode_video_frame, fftec_encode_audio_frame);
    fftec_close_input(input);
    fftec_close_output(output);
    return 0;
}