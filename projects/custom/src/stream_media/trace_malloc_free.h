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
#ifndef TRACE_MALLOC_FREE_H
#define TRACE_MALLOC_FREE_H

#ifdef __cplusplus
extern "C" {
#endif

void trace_malloc_free_create();
void trace_malloc_free_destroy();

void *__wrap_malloc(size_t size, const char *file, uint32_t line);
void __wrap_free(void *ptr, const char *file, uint32_t line);

#ifdef TRACE_MALLOC
#define malloc(size) __wrap_malloc(size, __FILE__, __LINE__)
#endif

#ifdef TRACE_FREE
#define free(ptr) __wrap_free(ptr, __FILE__, __LINE__)
#endif

#ifdef __cplusplus
}
#endif

#endif /* TRACE_MALLOC_FREE_H */
