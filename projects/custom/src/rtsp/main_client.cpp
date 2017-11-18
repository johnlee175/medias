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
#include "rtsp_ffmpeg_client.h"
#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

static void on_frame_rtsp(uint8_t *data[8], int line_size[8],
                          uint32_t width, uint32_t height, int64_t pts_millis) {
    cv::Mat image(height, width, CV_8UC3, data[0]);
    cv::imshow("Image Window", image);
    cv::waitKey(1);
}

static void test_rtsp(const char *url) {
    cv::namedWindow("Image Window", cv::WINDOW_AUTOSIZE);

    RtspClient *client = open_rtsp(url, AV_PIX_FMT_BGR24, on_frame_rtsp);
    if (client) {
        loop_read_rtsp_frame(client);
        close_rtsp(client);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cout << "Need a argument that indicate rtsp URL" << std::endl;
        return 0;
    }
    test_rtsp(argv[1]);
    return 0;
}