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
#ifndef __JOHN_OBJECT_POOL_H__
#define __JOHN_OBJECT_POOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct JohnObjectDelegate {
    void *user_client_params;
    void *(*create_func)(void *user_client_params);
    void (*destroy_func)(void *object, void *user_client_params);
    void (*reset_func)(void *object, void *user_client_params);
} JohnObjectDelegate;

typedef struct JohnObjectPool JohnObjectPool;


JohnObjectPool *john_object_pool_create(uint32_t max_size, uint32_t pool_size, uint32_t prepare_size,
                                        JohnObjectDelegate *delegate);
void john_object_pool_destroy(JohnObjectPool *object_pool);
void *john_object_pool_obtain(JohnObjectPool *object_pool);
void john_object_pool_recycle(JohnObjectPool *object_pool, void *object);

#ifdef __cplusplus
}
#endif

#endif /* __JOHN_OBJECT_POOL_H__ */
