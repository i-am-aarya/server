#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "queue.h"
#include <pthread.h>
#include <stdatomic.h>

typedef struct {
  queue_t *queue;

  pthread_t *threads;
  size_t thread_count;
  size_t min_threads;
  size_t max_threads;

  _Atomic bool shutdown;
  _Atomic size_t active_workers;

  // Adaptive metrics
  double avg_wait_ms; // Exponential moving average
  double alpha;       // EMA smoothing factor (0.3)
  uint64_t last_scale_time;
  uint64_t scale_cooldown_ms;

  pthread_mutex_t scale_mutex;

} thread_pool_t;

bool pool_init(thread_pool_t *p, queue_t *q, size_t min, size_t max);
void pool_submit(thread_pool_t *p, job_t job);
void pool_shutdown(thread_pool_t *p);

// Internal
void *worker_thread(void *arg);
bool pool_scale_up(thread_pool_t *p);
bool pool_scale_down(thread_pool_t *p);

#endif
