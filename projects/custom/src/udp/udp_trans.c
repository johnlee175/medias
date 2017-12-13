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
#include "udp_trans.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define LOGW(...) fprintf(stdout, __VA_ARGS__)

static int no_loop_port = -1;
static int closed_port = -1;

void udp_trans_init(const char *config) {
    FILE *file = fopen(config, "r");
    if (!file) {
        LOGW("open config file failed\n");
        return;
    }
    int file_length;
    if ((file_length = fseek(file , 0 , SEEK_END)) <= 0) {
        LOGW("get config file length failed\n");
        return;
    }
    char buffer[file_length];
    if (fread(buffer, (size_t)file_length, 1, file) != (size_t) file_length) {
        LOGW("read config file content failed\n");
        return;
    }
    fclose(file);
    sscanf(buffer, "local_port=%d;\nremote_port=%d;\nsend_buffer_size=%d;\nreceive_buffer_size=%d;\n",
    &local_port, &remote_port, &send_buffer_size, &receive_buffer_size);
}

void receive_udp_request(int port, on_udp_request callback) {
    closed_port = -1;

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOGW("create socket failed with %d\n", sockfd);
        perror("create socket failed");
        exit(-1);
    }

    int x = receive_buffer_size;
    if ((x = setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &x, sizeof(int))) < 0) {
        LOGW("set socket receive buffer size failed with %d\n", x);
        perror("set socket receive buffer size failed");
        exit(-1);
    }

    struct sockaddr_in local_addr;
    bzero(&local_addr, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(port);

    x = 1;
    if ((x = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &x, sizeof(int))) < 0) {
        LOGW("reuse socket address failed with %d\n", x);
        perror("reuse socket address failed");
        exit(-1);
    }

    int code;
    if ((code = bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr))) < 0) {
        LOGW("bind socket failed with %d\n", code);
        perror("bind socket failed");
        exit(-1);
    }

    char buffer[receive_buffer_size];
    struct sockaddr_in remote_addr;
    socklen_t remote_addr_size;
    ssize_t length;

    while (no_loop_port != port) {
        bzero(&remote_addr, sizeof(remote_addr));
        if ((length = recvfrom(sockfd, buffer, (size_t) receive_buffer_size, 0,
                               (struct sockaddr *)&remote_addr, &remote_addr_size)) < 0) {
            LOGW("receive udp data failed with %ld\n", length);
            perror("receive udp data failed");
            exit(-1);
        }
        const char *remote_ip_host = inet_ntoa(remote_addr.sin_addr);
        LOGW("received packet from %s\n", remote_ip_host);
        if (callback(remote_ip_host, buffer, (int) length) < 0) {
            LOGW("quit receive udp data\n");
            break;
        }
    }

    close(sockfd);

    closed_port = port;
}

void send_udp_response(const char *ip_host, int port,
                              const char *data, int data_size) {
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOGW("create socket failed with %d\n", sockfd);
        perror("create socket failed");
        exit(-1);
    }

    int x = send_buffer_size;
    if ((x = setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &x, sizeof(int))) < 0) {
        LOGW("set socket send buffer size failed with %d\n", x);
        perror("set socket send buffer size failed");
        exit(-1);
    }

    struct sockaddr_in remote_addr;
    bzero(&remote_addr, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_addr.s_addr = inet_addr(ip_host);
    remote_addr.sin_port = htons(port);

    ssize_t code;
    if ((code = sendto(sockfd, data, (size_t) data_size, 0,
                       (struct sockaddr *)&remote_addr, sizeof(remote_addr))) < 0) {
        LOGW("send udp data failed with %ld\n", code);
        perror("send udp data failed");
        exit(-1);
    }

    close(sockfd);
}
