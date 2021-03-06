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
#ifndef __JOHN_STACK_H__
#define __JOHN_STACK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct JohnStack JohnStack;

JohnStack *john_stack_create(uint32_t capacity);
void john_stack_destroy(JohnStack *stack);
bool john_stack_push(JohnStack *stack, void *data);
void *john_stack_pop(JohnStack *stack);
void *john_stack_peek(JohnStack *stack);
bool john_stack_is_full(JohnStack *stack);
bool john_stack_is_empty(JohnStack *stack);
void john_stack_clear(JohnStack *stack);

#ifdef __cplusplus
}
#endif

#endif /* __JOHN_STACK_H__ */
