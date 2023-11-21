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

#include "rpa_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

// uncomment to print debug messages
//#define QUEUE_DEBUG

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

#ifdef QUEUE_DEBUG
static void Q_DBG(const char*msg, rpa_queue_t *q) {
  fprintf(stderr, "#%d in %d out %d\t%s\n",
          q->nelts, q->in, q->out,
          msg
          );
}
#else
#define Q_DBG(x,y)
#endif

/**
 * @brief Macro to check if the rpa_queue_t is full.
 *
 * This macro checks if the number of elements in the queue is equal to the maximum
 * size of the queue, indicating that the queue is full.
 *
 * @param queue Pointer to the rpa_queue_t instance.
 * @return 1 if the queue is full, 0 otherwise.
 */
#define MC_rpa_queue_full(queue) ((queue)->nelts == (queue)->bounds)

/**
 * @brief Macro to get the number of free slots in the rpa_queue_t.
 *
 * This macro calculates the number of free slots in the queue by subtracting
 * the current number of elements from the maximum size of the queue.
 *
 * @param queue Pointer to the rpa_queue_t instance.
 * @return The number of free slots in the queue.
 */
#define MC_rpa_queue_get_free(queue) (((queue)->bounds) - ((queue)->nelts))

/**
 * @brief Macro to get the number of taken slots in the rpa_queue_t.
 *
 * This macro retrieves the current number of elements in the queue, indicating
 * the number of slots that have been taken.
 *
 * @param queue Pointer to the rpa_queue_t instance.
 * @return The number of taken slots in the queue.
 */
#define MC_rpa_queue_get_taken(queue) ((queue)->nelts)

/**
 * @brief Macro to check if the rpa_queue_t is empty.
 *
 * This macro checks if the number of elements in the queue is zero, indicating
 * that the queue is empty.
 *
 * @param queue Pointer to the rpa_queue_t instance.
 * @return 1 if the queue is empty, 0 otherwise.
 */
#define MC_rpa_queue_empty(queue) ((queue)->nelts == 0)

/**
 * @brief Macro to check if the rpa_queue_t is full.
 *
 * This macro checks if the number of elements in the queue is equal to the maximum
 * size of the queue, indicating that the queue is full.
 *
 * @param queue Pointer to the rpa_queue_t instance.
 * @return 1 if the queue is full, 0 otherwise.
 */
#define MC_rpa_queue_full(queue) ((queue)->nelts == (queue)->bounds)

/**
 * @brief Macro to get the number of free slots in the rpa_queue_t.
 *
 * This macro calculates the number of free slots in the queue by subtracting
 * the current number of elements from the maximum size of the queue.
 *
 * @param queue Pointer to the rpa_queue_t instance.
 * @return The number of free slots in the queue.
 */
#define MC_rpa_queue_get_free(queue) (((queue)->bounds) - ((queue)->nelts))

/**
 * @brief Macro to get the number of taken slots in the rpa_queue_t.
 *
 * This macro retrieves the current number of elements in the queue, indicating
 * the number of slots that have been taken.
 *
 * @param queue Pointer to the rpa_queue_t instance.
 * @return The number of taken slots in the queue.
 */
#define MC_rpa_queue_get_taken(queue) ((queue)->nelts)

/**
 * @brief Macro to check if the rpa_queue_t is empty.
 *
 * This macro checks if the number of elements in the queue is zero, indicating
 * that the queue is empty.
 *
 * @param queue Pointer to the rpa_queue_t instance.
 * @return 1 if the queue is empty, 0 otherwise.
 */

bool rpa_queue_empty(rpa_queue_t *queue)
{
	return MC_rpa_queue_empty(queue);
}

bool rpa_queue_full(queue)
{
	return MC_rpa_queue_full(queue);
}

unsigned rpa_queue_get_free(queue)
{
	return MC_rpa_queue_get_free(queue);
}

unsigned rpa_queue_get_taken(queue)
{
	return MC_rpa_queue_get_taken(queue);
}


static bool _rpa_queue_timedpop(rpa_queue_t *queue, void **data, int wait_ms, bool remove_from_queue);

static void set_timeout(struct timespec * abstime, int wait_ms)
{
  clock_gettime(CLOCK_REALTIME, abstime);
  /* add seconds */
  abstime->tv_sec += (wait_ms / 1000);
  /* add and carry microseconds */
  long ms = abstime->tv_nsec / 1000000L;
  ms += wait_ms % 1000;
  while (ms > 1000) {
    ms -= 1000;
    abstime->tv_sec += 1;
  }
  abstime->tv_nsec = ms * 1000000L;
}

/**
 * Callback routine that is called to destroy this
 * rpa_queue_t when its pool is destroyed.
 */
void rpa_queue_destroy(rpa_queue_t * queue)
{
  /* Ignore errors here, we can't do anything about them anyway. */
  pthread_cond_destroy(queue->not_empty);
  pthread_cond_destroy(queue->not_full);
  pthread_mutex_destroy(queue->one_big_mutex);
}

void rpa_queue_free(rpa_queue_t * queue) 
{
  if (queue->data) free(queue->data);
  if (queue->not_empty) free(queue->not_empty);
  if (queue->not_full) free(queue->not_full);
  if (queue->one_big_mutex) free(queue->one_big_mutex);
  free(queue);
}

/**
 * Initialize the rpa_queue_t.
 */
bool rpa_queue_create(rpa_queue_t **q, uint32_t queue_capacity)
{
  rpa_queue_t *queue;
  queue = malloc(sizeof(rpa_queue_t));
  if (!queue) {
    return false;
  }
  *q = queue;
  memset(queue, 0, sizeof(rpa_queue_t));

  if (!(queue->one_big_mutex = malloc(sizeof(pthread_mutex_t)))) return false;
  if (!(queue->not_empty = malloc(sizeof(pthread_cond_t)))) return false;
  if (!(queue->not_full = malloc(sizeof(pthread_cond_t)))) return false;

  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  int rv = pthread_mutex_init(queue->one_big_mutex, &attr);
  if (rv != 0) {
    Q_DBG("pthread_mutex_init failed", queue);
    goto error;
  }

  rv = pthread_cond_init(queue->not_empty, NULL);
  if (rv != 0) {
    Q_DBG("pthread_cond_init not_empty failed", queue);
    goto error;
  }

  rv = pthread_cond_init(queue->not_full, NULL);
  if (rv != 0) {
    Q_DBG("pthread_cond_init not_full failed", queue);
    goto error;
  }

  /* Set all the data in the queue to NULL */
  queue->data = malloc(queue_capacity * sizeof(void*));
  queue->bounds = queue_capacity;
  queue->nelts = 0;
  queue->in = 0;
  queue->out = 0;
  queue->terminated = 0;
  queue->full_waiters = 0;
  queue->empty_waiters = 0;

  return true;

error:
  free(queue);
  return false;
}

/**
 * Push new data onto the queue. Blocks if the queue is full. Once
 * the push operation has completed, it signals other threads waiting
 * in rpa_queue_pop() that they may continue consuming sockets.
 */
bool rpa_queue_push(rpa_queue_t *queue, void *data)
{
  return rpa_queue_timedpush(queue, data, RPA_WAIT_FOREVER);
}

bool rpa_queue_timedpush(rpa_queue_t *queue, void *data, int wait_ms)
{
  bool rv;

  if (wait_ms == RPA_WAIT_NONE) return rpa_queue_trypush(queue, data);

  if (queue->terminated) {
    return false; /* no more elements ever again */
  }

  rv = pthread_mutex_lock(queue->one_big_mutex);
  if (rv != 0) {
    Q_DBG("failed to lock mutex", queue);
    return false;
  }

  if (MC_rpa_queue_full(queue)) {
    if (!queue->terminated) {
      queue->full_waiters++;
      if (wait_ms == RPA_WAIT_FOREVER) {
        rv = pthread_cond_wait(queue->not_full, queue->one_big_mutex);
      } else {
        struct timespec abstime;
        set_timeout(&abstime, wait_ms);
        rv = pthread_cond_timedwait(queue->not_full, queue->one_big_mutex,
          &abstime);
      }
      queue->full_waiters--;
      if (rv != 0) {
        pthread_mutex_unlock(queue->one_big_mutex);
        return false;
      }
    }
    /* If we wake up and it's still empty, then we were interrupted */
    if (MC_rpa_queue_full(queue)) {
      Q_DBG("queue full (intr)", queue);
      rv = pthread_mutex_unlock(queue->one_big_mutex);
      if (rv != 0) {
        return false;
      }
      if (queue->terminated) {
        return false; /* no more elements ever again */
      } else {
        return false; //EINTR;
      }
    }
  }

  queue->data[queue->in] = data;
  queue->in++;
  if (queue->in >= queue->bounds) {
    queue->in -= queue->bounds;
  }
  queue->nelts++;

  if (queue->empty_waiters) {
    Q_DBG("sig !empty", queue);
    rv = pthread_cond_signal(queue->not_empty);
    if (rv != 0) {
      pthread_mutex_unlock(queue->one_big_mutex);
      return false;
    }
  }

  pthread_mutex_unlock(queue->one_big_mutex);
  return true;
}

/**
 * Push new data onto the queue. If the queue is full, return RPA_EAGAIN. If
 * the push operation completes successfully, it signals other threads
 * waiting in rpa_queue_pop() that they may continue consuming sockets.
 */
bool rpa_queue_trypush(rpa_queue_t *queue, void *data)
{
  bool rv;

  if (queue->terminated) {
    return false; /* no more elements ever again */
  }

  rv = pthread_mutex_lock(queue->one_big_mutex);
  if (rv != 0) {
    return false;
  }

  if (MC_rpa_queue_full(queue)) {
    rv = pthread_mutex_unlock(queue->one_big_mutex);
    return false; //EAGAIN;
  }

  queue->data[queue->in] = data;
  queue->in++;
  if (queue->in >= queue->bounds) {
    queue->in -= queue->bounds;
  }
  queue->nelts++;

  if (queue->empty_waiters) {
    Q_DBG("sig !empty", queue);
    rv = pthread_cond_signal(queue->not_empty);
    if (rv != 0) {
      pthread_mutex_unlock(queue->one_big_mutex);
      return false;
    }
  }

  pthread_mutex_unlock(queue->one_big_mutex);
  return true;
}

/**
 * not thread safe
 */
uint32_t rpa_queue_size(rpa_queue_t *queue) {
  return queue->nelts;
}

bool rpa_queue_timedpeek(rpa_queue_t *queue, void **data, int wait_ms)
{
  return _rpa_queue_timedpop(queue, data, wait_ms, false);
}


/**
 * Retrieves the next item from the queue. If there are no
 * items available, it will block until one becomes available.
 * Once retrieved, the item is placed into the address specified by
 * 'data'.
 */
bool rpa_queue_pop(rpa_queue_t *queue, void **data)
{
  return rpa_queue_timedpop(queue, data, RPA_WAIT_FOREVER);
}

bool rpa_queue_timedpop(rpa_queue_t *queue, void **data, int wait_ms)
{
	return _rpa_queue_timedpop(queue, data, wait_ms, true);
}

static bool _rpa_queue_timedpop(rpa_queue_t *queue, void **data, int wait_ms, bool remove_from_queue)
{
  bool rv;

  if (wait_ms == RPA_WAIT_NONE) return rpa_queue_trypop(queue, data);

  if (queue->terminated) {
    return false; /* no more elements ever again */
  }

  rv = pthread_mutex_lock(queue->one_big_mutex);
  if (rv != 0) {
    return false;
  }

  /* Keep waiting until we wake up and find that the queue is not empty. */
  if (MC_rpa_queue_empty(queue)) {
    if (!queue->terminated) {
      queue->empty_waiters++;
      if (wait_ms == RPA_WAIT_FOREVER) {
        rv = pthread_cond_wait(queue->not_empty, queue->one_big_mutex);
      } else {
        struct timespec abstime;
        set_timeout(&abstime, wait_ms);
        rv = pthread_cond_timedwait(queue->not_empty, queue->one_big_mutex,
          &abstime);
      }
      queue->empty_waiters--;
      if (rv != 0) {
        pthread_mutex_unlock(queue->one_big_mutex);
        return false;
      }
    }
    /* If we wake up and it's still empty, then we were interrupted */
    if (MC_rpa_queue_empty(queue)) {
      Q_DBG("queue empty (intr)", queue);
      rv = pthread_mutex_unlock(queue->one_big_mutex);
      if (rv != 0) {
        return false;
      }
      if (queue->terminated) {
        return false; /* no more elements ever again */
      } else {
        return false; //EINTR;
      }
    }
  }

  *data = queue->data[queue->out];
  if(remove_from_queue)
  {
	  queue->nelts--;

	  queue->out++;
	  if (queue->out >= queue->bounds)
	  {
	    queue->out -= queue->bounds;
	  }
  }

  if (queue->full_waiters) {
    Q_DBG("signal !full", queue);
    rv = pthread_cond_signal(queue->not_full);
    if (rv != 0) {
      pthread_mutex_unlock(queue->one_big_mutex);
      return false;
    }
  }

  //function_return:

  pthread_mutex_unlock(queue->one_big_mutex);
  return true;
}

/**
 * Retrieves the next item from the queue. If there are no
 * items available, return RPA_EAGAIN.  Once retrieved,
 * the item is placed into the address specified by 'data'.
 */
bool rpa_queue_trypop(rpa_queue_t *queue, void **data)
{
  bool rv;

  if (queue->terminated) {
    return false; /* no more elements ever again */
  }

  rv = pthread_mutex_lock(queue->one_big_mutex);
  if (rv != 0) {
    return false;
  }

  if (MC_rpa_queue_empty(queue)) {
    rv = pthread_mutex_unlock(queue->one_big_mutex);
    return false; //EAGAIN;
  }

  *data = queue->data[queue->out];
  queue->nelts--;

  queue->out++;
  if (queue->out >= queue->bounds) {
    queue->out -= queue->bounds;
  }
  if (queue->full_waiters) {
    Q_DBG("signal !full", queue);
    rv = pthread_cond_signal(queue->not_full);
    if (rv != 0) {
      pthread_mutex_unlock(queue->one_big_mutex);
      return false;
    }
  }

  pthread_mutex_unlock(queue->one_big_mutex);
  return true;
}

bool rpa_queue_interrupt_all(rpa_queue_t *queue)
{
  bool rv;
  Q_DBG("intr all", queue);
  if ((rv = pthread_mutex_lock(queue->one_big_mutex)) != 0) {
    return false;
  }
  pthread_cond_broadcast(queue->not_empty);
  pthread_cond_broadcast(queue->not_full);

  if ((rv = pthread_mutex_unlock(queue->one_big_mutex)) != 0) {
    return false;
  }

  return true;
}

bool rpa_queue_term(rpa_queue_t *queue)
{
  bool rv;

  if ((rv = pthread_mutex_lock(queue->one_big_mutex)) != 0) {
    return false;
  }

  /* we must hold one_big_mutex when setting this... otherwise,
   * we could end up setting it and waking everybody up just after a
   * would-be popper checks it but right before they block
   */
  queue->terminated = 1;
  if ((rv = pthread_mutex_unlock(queue->one_big_mutex)) != 0) {
    return false;
  }
  return rpa_queue_interrupt_all(queue);
}
