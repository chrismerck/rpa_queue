/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef RPA_QUEUE_H
#define RPA_QUEUE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

#define RPA_WAIT_NONE     0
#define RPA_WAIT_FOREVER  -1

/**
 * @file rpa_queue.h
 * @brief Thread Safe FIFO bounded queue
 * @note Since most implementations of the queue are backed by a condition
 * variable implementation, it isn't available on systems without threads.
 * Although condition variables are sometimes available without threads.
 */

/**
 * @defgroup RPA_Util_FIFO Thread Safe FIFO bounded queue
 * @ingroup RPA_Util
 * @{
 */

/**
 * @brief Opaque structure representing a thread-safe circular queue.
 */
typedef struct rpa_queue_t rpa_queue_t;

/**
 * @struct rpa_queue_t
 * @brief Opaque structure representing a thread-safe circular queue.
 *
 * This structure defines a thread-safe circular queue that supports concurrent access
 * by multiple threads. The queue uses a fixed-size array to store elements and is designed
 * to handle both producer and consumer threads efficiently.
 */
struct rpa_queue_t {
  void **data;                  /**< Array to store elements */
  volatile uint32_t nelts;      /**< Number of elements in the queue */
  uint32_t in;                  /**< Next empty location in the queue */
  uint32_t out;                 /**< Next filled location in the queue */
  uint32_t bounds;              /**< Maximum size of the queue */
  uint32_t full_waiters;        /**< Number of threads waiting on a full queue */
  uint32_t empty_waiters;       /**< Number of threads waiting on an empty queue */
  pthread_mutex_t *one_big_mutex;/**< Mutex for controlling access to the queue */
  pthread_cond_t *not_empty;    /**< Condition variable for signaling non-empty queue */
  pthread_cond_t *not_full;     /**< Condition variable for signaling non-full queue */
  int terminated;               /**< Flag indicating whether the queue is terminated */
};

/**
 * @brief Macro to check if the rpa_queue_t is full.
 *
 * This macro checks if the number of elements in the queue is equal to the maximum
 * size of the queue, indicating that the queue is full.
 *
 * @param queue Pointer to the rpa_queue_t instance.
 * @return 1 if the queue is full, 0 otherwise.
 */
#define rpa_queue_full(queue) ((queue)->nelts == (queue)->bounds)

/**
 * @brief Macro to get the number of free slots in the rpa_queue_t.
 *
 * This macro calculates the number of free slots in the queue by subtracting
 * the current number of elements from the maximum size of the queue.
 *
 * @param queue Pointer to the rpa_queue_t instance.
 * @return The number of free slots in the queue.
 */
#define rpa_queue_get_free(queue) (((queue)->bounds) - ((queue)->nelts))

/**
 * @brief Macro to get the number of taken slots in the rpa_queue_t.
 *
 * This macro retrieves the current number of elements in the queue, indicating
 * the number of slots that have been taken.
 *
 * @param queue Pointer to the rpa_queue_t instance.
 * @return The number of taken slots in the queue.
 */
#define rpa_queue_get_taken(queue) ((queue)->nelts)

/**
 * @brief Macro to check if the rpa_queue_t is empty.
 *
 * This macro checks if the number of elements in the queue is zero, indicating
 * that the queue is empty.
 *
 * @param queue Pointer to the rpa_queue_t instance.
 * @return 1 if the queue is empty, 0 otherwise.
 */
#define rpa_queue_empty(queue) ((queue)->nelts == 0)

/**
 * create a FIFO queue
 * @param queue The new queue
 * @param queue_capacity maximum size of the queue
 * @param a pool to allocate queue from
 */
bool rpa_queue_create(rpa_queue_t **queue, uint32_t queue_capacity);

/**
 * push/add an object to the queue, blocking if the queue is already full
 *
 * @param queue the queue
 * @param data the data
 * @returns RPA_EINTR the blocking was interrupted (try again)
 * @returns RPA_EOF the queue has been terminated
 * @returns RPA_SUCCESS on a successful push
 */
bool rpa_queue_push(rpa_queue_t *queue, void *data);

/**
 * push/add an object to the queue, blocking if the queue is already full
 *
 * @param queue         the queue
 * @param data          the data
 * @param wait_ms       milliseconds to wait
 * @returns RPA_EINTR   the blocking was interrupted (try again)
 * @returns RPA_EOF     the queue has been terminated
 * @returns RPA_SUCCESS on a successful push
 */
bool rpa_queue_timedpush(rpa_queue_t *queue, void *data, int wait_ms);

/**
 * peek an object from the queue without removing it from the queue, blocking if the queue is already empty
 *
 * @param queue         the queue
 * @param data          the data
 * @param wait_ms       milliseconds to wait
 * @returns RPA_EINTR   the blocking was interrupted (try again)
 * @returns RPA_EOF     if the queue has been terminated
 * @returns RPA_SUCCESS on a successful pop
 */
bool rpa_queue_timedpeek(rpa_queue_t *queue, void **data, int wait_ms);

/**
 * pop/get an object from the queue, blocking if the queue is already empty
 *
 * @param queue the queue
 * @param data the data
 * @returns RPA_EINTR the blocking was interrupted (try again)
 * @returns RPA_EOF if the queue has been terminated
 * @returns RPA_SUCCESS on a successful pop
 */
bool rpa_queue_pop(rpa_queue_t *queue, void **data);

/**
 * pop/get an object from the queue, blocking if the queue is already empty
 *
 * @param queue         the queue
 * @param data          the data
 * @param wait_ms       milliseconds to wait
 * @returns RPA_EINTR   the blocking was interrupted (try again)
 * @returns RPA_EOF     if the queue has been terminated
 * @returns RPA_SUCCESS on a successful pop
 */
bool rpa_queue_timedpop(rpa_queue_t *queue, void **data, int wait_ms);

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
bool rpa_queue_trypush(rpa_queue_t *queue, void *data);

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
bool rpa_queue_trypop(rpa_queue_t *queue, void **data);

/**
 * returns the size of the queue.
 *
 * @warning this is not threadsafe, and is intended for reporting/monitoring
 * of the queue.
 * @param queue the queue
 * @returns the size of the queue
 */
uint32_t rpa_queue_size(rpa_queue_t *queue);

/**
 * interrupt all the threads blocking on this queue.
 *
 * @param queue the queue
 */
bool rpa_queue_interrupt_all(rpa_queue_t *queue);

/**
 * terminate the queue, sending an interrupt to all the
 * blocking threads
 *
 * @param queue the queue
 */
bool rpa_queue_term(rpa_queue_t *queue);

/**
 * destroy queue
 * @param  queue
 * @return     always true
 */
void rpa_queue_destroy(rpa_queue_t * queue);

/** free queue memory -- call after destroy 
 * @note we do not free in destroy() to follow pthreads convention
*/
void rpa_queue_free(rpa_queue_t * queue);

#endif /* RPAQUEUE_H */
