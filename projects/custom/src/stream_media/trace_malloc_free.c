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
 * @version 2017-12-10
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "common.h"

uint64_t alloc_count = 0;
pthread_mutex_t alloc_count_lock;

void trace_malloc_free_create() {
    pthread_mutex_init(&alloc_count_lock, NULL);
}

void trace_malloc_free_destroy() {
    pthread_mutex_destroy(&alloc_count_lock);
}

void *__wrap_malloc(size_t size, const char *file, uint32_t line) {
    pthread_mutex_lock(&alloc_count_lock);
    ++alloc_count;
    pthread_mutex_unlock(&alloc_count_lock);
    void *ptr = malloc(size);
    LOGW("__wrap_malloc[%lld]: file=%s; line=%d; pointer=%p; size=%zu;\n",
           alloc_count, file, line, ptr, size);
    return ptr;
}

void __wrap_free(void *ptr, const char *file, uint32_t line) {
    pthread_mutex_lock(&alloc_count_lock);
    --alloc_count;
    pthread_mutex_unlock(&alloc_count_lock);
    LOGW("__wrap_free[%lld]: file=%s; line=%d; pointer=%p;\n",
           alloc_count, file, line, ptr);
    free(ptr);
}
