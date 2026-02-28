#include "thread_pool.h"
#include "config.h"
#include "http.h"
#include "utils.h"
#include <stdlib.h>
#include <unistd.h>

bool pool_init(thread_pool_t *p, queue_t *q, size_t min, size_t max) {
  p->queue = q;
  p->min_threads = min;
  p->max_threads = max;
  p->thread_count = min;
  p->avg_wait_ms = 0;
  p->alpha = 0.3;
  p->last_scale_time = 0;
  p->scale_cooldown_ms = 1000;

  atomic_init(&p->shutdown, false);
  atomic_init(&p->active_workers, 0);
  pthread_mutex_init(&p->scale_mutex, NULL);

  p->threads = calloc(max, sizeof(pthread_t));
  if (!p->threads)
    return false;

  for (size_t i = 0; i < min; i++) {
    pthread_create(&p->threads[i], NULL, worker_thread, p);
  }

  log_info("Thread pool initialized: %zu workers (min: %zu, max: %zu)", min,
           min, max);
  return true;
}

void pool_submit(thread_pool_t *p, job_t job) {
  while (!queue_enqueue(p->queue, job)) {
    if (p->thread_count < p->max_threads) {
      pool_scale_up(p);
    }
    usleep(1000);
  }
}

void *worker_thread(void *arg) {
  thread_pool_t *p = arg;
  job_t job;

  while (!atomic_load_explicit(&p->shutdown, memory_order_relaxed)) {
    bool got_work = false;
    for (int spin = 0; spin < 1000 && !got_work; spin++) {
      got_work = queue_dequeue(p->queue, &job);
      if (!got_work)
        __asm__ volatile("pause");
    }

    if (!got_work) {
      usleep(1000);
      continue;
    }

    uint64_t wait_time = time_ms() - job.enqueue_time;

    pthread_mutex_lock(&p->scale_mutex);
    p->avg_wait_ms = p->alpha * wait_time + (1 - p->alpha) * p->avg_wait_ms;
    bool should_scale = (wait_time > 50 && p->thread_count < p->max_threads);
    pthread_mutex_unlock(&p->scale_mutex);

    if (should_scale) {
      pool_scale_up(p);
    }

    atomic_fetch_add_explicit(&p->active_workers, 1, memory_order_relaxed);
    http_handle_job(&job);
    atomic_fetch_sub_explicit(&p->active_workers, 1, memory_order_relaxed);
  }

  return NULL;
}

bool pool_scale_up(thread_pool_t *p) {
  pthread_mutex_lock(&p->scale_mutex);

  uint64_t now = time_ms();
  if (now - p->last_scale_time < p->scale_cooldown_ms) {
    pthread_mutex_unlock(&p->scale_mutex);
    return false;
  }

  if (p->thread_count >= p->max_threads) {
    pthread_mutex_unlock(&p->scale_mutex);
    return false;
  }

  size_t new_idx = p->thread_count++;
  if (pthread_create(&p->threads[new_idx], NULL, worker_thread, p) != 0) {
    p->thread_count--;
    pthread_mutex_unlock(&p->scale_mutex);
    return false;
  }

  p->last_scale_time = now;
  log_info("Scaled up: %zu threads (avg wait: %.2fms)", p->thread_count,
           p->avg_wait_ms);

  pthread_mutex_unlock(&p->scale_mutex);
  return true;
}

bool pool_scale_down(thread_pool_t *p) {
  (void)p;
  return false;
}

void pool_shutdown(thread_pool_t *p) {
  atomic_store_explicit(&p->shutdown, true, memory_order_release);

  for (size_t i = 0; i < p->thread_count; i++) {
    pthread_join(p->threads[i], NULL);
  }

  free(p->threads);
  pthread_mutex_destroy(&p->scale_mutex);

  log_info("Thread pool shutdown complete");
}
