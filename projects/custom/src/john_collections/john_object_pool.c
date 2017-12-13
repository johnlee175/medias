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
 * Note: We did not check if the pthread related function call succeeded
 * @author John Kenrinus Lee
 * @version 2017-11-10
 */
#include <pthread.h>
#include <stdlib.h>
#include <memory.h>
#include "john_stack.h"
#include "john_object_pool.h"

struct JohnObjectPool {
    uint32_t max_size;
    uint32_t pool_size;
    uint32_t prepare_size;
    uint32_t alloc_size;

    JohnStack *stack;

    void *user_client_params;
    void *(*create_func)(void *user_client_params);
    void (*destroy_func)(void *object, void *user_client_params);
    void (*reset_func)(void *object, void *user_client_params);

    pthread_mutex_t lock;
};

static inline bool john_object_pool_is_empty(JohnObjectPool *object_pool) {
    bool result;
    pthread_mutex_lock(&object_pool->lock);
    result = john_stack_is_empty(object_pool->stack);
    pthread_mutex_unlock(&object_pool->lock);
    return result;
}

static inline bool john_object_pool_is_full(JohnObjectPool *object_pool) {
    bool result;
    pthread_mutex_lock(&object_pool->lock);
    result = john_stack_is_full(object_pool->stack);
    pthread_mutex_unlock(&object_pool->lock);
    return result;
}

JohnObjectPool *john_object_pool_create(uint32_t max_size, uint32_t pool_size, uint32_t prepare_size,
                                        JohnObjectDelegate *delegate) {
    if (max_size < pool_size || pool_size < prepare_size || max_size < prepare_size) {
        return NULL;
    }

    JohnObjectPool *object_pool = (JohnObjectPool *) malloc(sizeof(JohnObjectPool));
    if (object_pool) {
        memset(object_pool, 0, sizeof(JohnObjectPool));
        object_pool->max_size = max_size;
        object_pool->pool_size = pool_size;
        object_pool->prepare_size = prepare_size;

        object_pool->stack = john_stack_create(pool_size);

        object_pool->user_client_params = delegate->user_client_params;
        object_pool->create_func = delegate->create_func;
        object_pool->destroy_func = delegate->destroy_func;
        object_pool->reset_func = delegate->reset_func;

        pthread_mutex_init(&object_pool->lock, NULL);

        pthread_mutex_lock(&object_pool->lock);
        for (uint32_t i = 0; i < prepare_size; ++i) {
            john_stack_push(object_pool->stack,
                            object_pool->create_func(object_pool->user_client_params));
            ++object_pool->alloc_size;
        }
        pthread_mutex_unlock(&object_pool->lock);
    }
    return object_pool;
}

void john_object_pool_destroy(JohnObjectPool *object_pool) {
    if (object_pool) {
        object_pool->user_client_params = NULL;
        pthread_mutex_destroy(&object_pool->lock);
        memset(&object_pool->lock, 0, sizeof(pthread_mutex_t));
        john_stack_destroy(object_pool->stack);
        object_pool->stack = NULL;
        free(object_pool);
    }
}

void *john_object_pool_obtain(JohnObjectPool *object_pool) {
    if (object_pool) {
        void *object;
        pthread_mutex_lock(&object_pool->lock);
        if (john_object_pool_is_empty(object_pool) && object_pool->alloc_size < object_pool->max_size) {
            object = object_pool->create_func(object_pool->user_client_params);
            ++object_pool->alloc_size;
        } else {
            object = john_stack_pop(object_pool->stack);
        }
        pthread_mutex_unlock(&object_pool->lock);
        return object;
    }
    return NULL;
}

/* User should avoid recycle repeatedly and avoid recycle object which had free */
void john_object_pool_recycle(JohnObjectPool *object_pool, void *object) {
    if (object_pool && object) {
        pthread_mutex_lock(&object_pool->lock);
        if (!john_object_pool_is_full(object_pool)) {
            object_pool->reset_func(object, object_pool->user_client_params);
            john_stack_push(object_pool->stack, object);
            return;
        }
        --object_pool->alloc_size;
        pthread_mutex_unlock(&object_pool->lock);
        free(object);
    }
}
