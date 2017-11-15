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
#ifndef UDP_TRANS_H
#define UDP_TRANS_H

#ifdef __cplusplus
extern "C" {
#endif

static int local_port = 32768;
static int remote_port = 5334;
static int send_buffer_size = 10240;
static int receive_buffer_size = 131072;

typedef int (*on_udp_request) (const char *ip_host, const char *data, int data_size);

void udp_trans_init(const char *config);
void receive_udp_request(int port, on_udp_request callback);
void send_udp_response(const char *ip_host, int port,
                       const char *data, int data_size);

#ifdef __cplusplus
}
#endif

#endif /* UDP_TRANS_H */