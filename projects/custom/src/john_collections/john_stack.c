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
#include <stdlib.h>
#include <memory.h>
#include "john_stack.h"

struct JohnStack {
    uint32_t capacity;
    int32_t pointer;
    void **array;
};

JohnStack *john_stack_create(uint32_t capacity) {
    JohnStack *stack = (JohnStack *) malloc(sizeof(JohnStack));
    if (stack) {
        memset(stack, 0, sizeof(JohnStack));
        stack->capacity = capacity;
        stack->pointer = -1;
        stack->array = (void **) malloc(sizeof(void *) * stack->capacity);
        memset(stack->array, 0, sizeof(void *) * stack->capacity);
    }
    return stack;
}

void john_stack_destroy(JohnStack *stack) {
    if (stack) {
        if (stack->array) {
            memset(stack->array, 0, sizeof(void *) * stack->capacity);
            free(stack->array);
        }
        memset(stack, 0, sizeof(JohnStack));
        free(stack);
    }
}

bool john_stack_push(JohnStack *stack, void *data) {
    if (stack && data && stack->pointer < (int32_t) stack->capacity) {
        stack->array[++stack->pointer] = data;
        return true;
    }
    return false;
}

void *john_stack_pop(JohnStack *stack) {
    if (stack && stack->pointer >= 0) {
        return stack->array[stack->pointer--];
    }
    return NULL;
}

void *john_stack_peek(JohnStack *stack) {
    if (stack && stack->pointer >= 0) {
        return stack->array[stack->pointer];
    }
    return NULL;
}

bool john_stack_is_full(JohnStack *stack) {
    return stack && stack->pointer == (int32_t) (stack->capacity - 1);
}

bool john_stack_is_empty(JohnStack *stack) {
    return stack && stack->pointer < 0;
}

void john_stack_clear(JohnStack *stack) {
    if (stack) {
        stack->pointer = -1;
        memset(stack->array, 0, sizeof(void *) * stack->capacity);
    }
}
