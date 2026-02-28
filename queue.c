#include "queue.h"
#include <stdlib.h>
#include <string.h>

bool queue_init(queue_t *q, size_t capacity) {
  q->buffer = calloc(capacity, sizeof(job_t));
  if (!q->buffer)
    return false;

  atomic_init(&q->head, 0);
  atomic_init(&q->tail, 0);
  q->capacity = capacity;

  return true;
}

void queue_destroy(queue_t *q) {
  free(q->buffer);
  q->buffer = NULL;
}

bool queue_enqueue(queue_t *q, job_t job) {
  size_t head = atomic_load_explicit(&q->head, memory_order_relaxed);
  size_t next_head = (head + 1) & (q->capacity - 1);

  if (next_head == atomic_load_explicit(&q->tail, memory_order_acquire)) {
    return false;
  }

  q->buffer[head] = job;
  atomic_store_explicit(&q->head, next_head, memory_order_release);

  return true;
}

bool queue_dequeue(queue_t *q, job_t *job) {
  size_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);

  while (1) {
    size_t head = atomic_load_explicit(&q->head, memory_order_acquire);

    if (tail == head) {
      return false;
    }

    job_t local_copy = q->buffer[tail];

    size_t next_tail = (tail + 1) & (q->capacity - 1);

    if (atomic_compare_exchange_weak_explicit(&q->tail, &tail, next_tail,
                                              memory_order_release,
                                              memory_order_relaxed)) {
      *job = local_copy;
      return true;
    }
  }
}

size_t queue_size_approx(queue_t *q) {
  size_t head = atomic_load_explicit(&q->head, memory_order_relaxed);
  size_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
  return (head - tail) & (q->capacity - 1);
}
