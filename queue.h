#ifndef QUEUE_H
#define QUEUE_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  int client_fd;
  uint64_t enqueue_time; // For adaptive pool metrics
  bool keep_alive;       // Connection persistence flag
  int timeout_ms;        // Keep-alive timeout
} job_t;

// typedef struct {
//   int client_fd;
//   uint64_t enqueue_time; // For adaptive pool metrics
// } job_t;

typedef struct {
  job_t *buffer;
  _Atomic size_t head; // Only producer writes
  _Atomic size_t tail; // Consumers CAS to claim
  size_t capacity;
  char _pad[64]; // Cache line padding (false sharing prevention)
} queue_t;

bool queue_init(queue_t *q, size_t capacity);
void queue_destroy(queue_t *q);

// Producer only (main thread)
bool queue_enqueue(queue_t *q, job_t job);

// Multi-consumer (workers)
bool queue_dequeue(queue_t *q, job_t *job);

size_t queue_size_approx(queue_t *q);

#endif
