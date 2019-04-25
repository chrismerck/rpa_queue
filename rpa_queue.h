/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef RPA_QUEUE_H
#define RPA_QUEUE_H

/**
 * @file rpa_queue.h
 * @brief Thread Safe FIFO bounded queue
 * @note Since most implementations of the queue are backed by a condition
 * variable implementation, it isn't available on systems without threads.
 * Although condition variables are sometimes available without threads.
 */

#include "apu.h"
#include "rpa_errno.h"
#include "rpa_pools.h"

#if RPA_HAS_THREADS

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @defgroup RPA_Util_FIFO Thread Safe FIFO bounded queue
 * @ingroup RPA_Util
 * @{
 */

/**
 * opaque structure
 */
typedef struct rpa_queue_t rpa_queue_t;

/** 
 * create a FIFO queue
 * @param queue The new queue
 * @param queue_capacity maximum size of the queue
 * @param a pool to allocate queue from
 */
APU_DECLARE(rpa_status_t) rpa_queue_create(rpa_queue_t **queue, 
                                           unsigned int queue_capacity, 
                                           rpa_pool_t *a);

/**
 * push/add an object to the queue, blocking if the queue is already full
 *
 * @param queue the queue
 * @param data the data
 * @returns RPA_EINTR the blocking was interrupted (try again)
 * @returns RPA_EOF the queue has been terminated
 * @returns RPA_SUCCESS on a successful push
 */
APU_DECLARE(rpa_status_t) rpa_queue_push(rpa_queue_t *queue, void *data);

/**
 * pop/get an object from the queue, blocking if the queue is already empty
 *
 * @param queue the queue
 * @param data the data
 * @returns RPA_EINTR the blocking was interrupted (try again)
 * @returns RPA_EOF if the queue has been terminated
 * @returns RPA_SUCCESS on a successful pop
 */
APU_DECLARE(rpa_status_t) rpa_queue_pop(rpa_queue_t *queue, void **data);

/**
 * push/add an object to the queue, returning immediately if the queue is full
 *
 * @param queue the queue
 * @param data the data
 * @returns RPA_EINTR the blocking operation was interrupted (try again)
 * @returns RPA_EAGAIN the queue is full
 * @returns RPA_EOF the queue has been terminated
 * @returns RPA_SUCCESS on a successful push
 */
APU_DECLARE(rpa_status_t) rpa_queue_trypush(rpa_queue_t *queue, void *data);

/**
 * pop/get an object to the queue, returning immediately if the queue is empty
 *
 * @param queue the queue
 * @param data the data
 * @returns RPA_EINTR the blocking operation was interrupted (try again)
 * @returns RPA_EAGAIN the queue is empty
 * @returns RPA_EOF the queue has been terminated
 * @returns RPA_SUCCESS on a successful pop
 */
APU_DECLARE(rpa_status_t) rpa_queue_trypop(rpa_queue_t *queue, void **data);

/**
 * returns the size of the queue.
 *
 * @warning this is not threadsafe, and is intended for reporting/monitoring
 * of the queue.
 * @param queue the queue
 * @returns the size of the queue
 */
APU_DECLARE(unsigned int) rpa_queue_size(rpa_queue_t *queue);

/**
 * interrupt all the threads blocking on this queue.
 *
 * @param queue the queue
 */
APU_DECLARE(rpa_status_t) rpa_queue_interrupt_all(rpa_queue_t *queue);

/**
 * terminate the queue, sending an interrupt to all the
 * blocking threads
 *
 * @param queue the queue
 */
APU_DECLARE(rpa_status_t) rpa_queue_term(rpa_queue_t *queue);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* RPA_HAS_THREADS */

#endif /* RPAQUEUE_H */
