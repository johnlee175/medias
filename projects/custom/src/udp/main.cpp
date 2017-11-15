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
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include "udp_trans.h"

using namespace std;
using namespace cv;

static int handle_message(const char *ip_host, const char *data, int data_size) {
    /*printf("%d, %d, %d, %d\n", data[0], data[1], data[2], data[3]);
    FILE *file = fopen("/Users/john/temp/temp/a.jpg", "w");
    fwrite(data + 4, (size_t)(data_size - 4), 1, file);
    fclose(file);*/

    vector<uchar> vec(data + 4, data + data_size);
    Mat jpeg_image = imdecode(Mat(vec), CV_LOAD_IMAGE_COLOR);

    /* TODO handle */

    const char *expression = "smile";
    char buffer[32] = { 0x42, 0x44, 0x46, 0x48, 0x0 };
    memcpy(buffer + 4, expression, strlen(expression));
    send_udp_response(ip_host, remote_port, buffer, sizeof(buffer));
    return 0;
}

int main() {
    udp_trans_init("./udp_trans.cfg");
    send_udp_response("10.0.1.109", remote_port, "hello", sizeof("hello"));
    receive_udp_request(local_port, handle_message);
    return 0;
}