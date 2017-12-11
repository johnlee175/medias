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
 * @version 2017-11-14
 */
#include "ffmpeg_url_client.h"
#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "john_synchronized_queue.h"

static JohnSynchronizedQueue *queue;
static bool quit = false;

static void on_frame(uint8_t *data[8], int line_size[8],
                          uint32_t width, uint32_t height, int64_t pts_millis) {
    cv::Mat *image = new cv::Mat(height, width, CV_8UC3, data[0]);
    cv::Mat *old = nullptr;
    john_synchronized_queue_enqueue(queue, image, reinterpret_cast<void **>(&old), -1);
    delete old;
}

static void *test_stream(void *data) {
    const char *url = static_cast<const char *>(data);
    FFmpegClient *client = open_media(url, AV_PIX_FMT_BGR24, on_frame);
    if (client) {
        loop_read_frame(client);
        close_media(client);
    }
    quit = true;
    return nullptr;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cout << "Need a argument that indicate rtsp/rtmp URL" << std::endl;
        return 0;
    }

    queue = john_synchronized_queue_create(40, true, nullptr);
    cv::namedWindow("Image Window", cv::WINDOW_AUTOSIZE);

    pthread_t stream_thread;
    pthread_create(&stream_thread, nullptr, test_stream, argv[1]);

    while (!quit) {
        cv::Mat *image = (cv::Mat *)john_synchronized_queue_dequeue(queue, 5000L);
        if (image) {
            cv::imshow("Image Window", *image);
            delete image;
        }
        if (cv::waitKey(1) == 27) {
            break;
        }
    }

    cv::destroyAllWindows();
    john_synchronized_queue_destroy(queue);

    exit(0);
}