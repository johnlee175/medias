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
#ifndef __COMMON_H__
#define __COMMON_H__

#include <inttypes.h>

#if defined(ANDROID) || defined(__ANDROID__)

#include <android/log.h>
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "System.err", __VA_ARGS__))

#else

#include <stdio.h>
#define LOGW(...)  ((void)fprintf(stderr, __VA_ARGS__))

#endif


#endif /* __COMMON_H__ */
