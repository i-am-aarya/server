#include "config.h"
#include "queue.h"
#include "server.h"
#include "thread_pool.h"
#include "utils.h"
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static queue_t queue;
static thread_pool_t pool;
static int server_fd = -1;

void signal_handler(int sig) {
  (void)sig;
  log_info("Shutdown signal received");
  if (server_fd >= 0)
    close(server_fd);
  pool_shutdown(&pool);
  queue_destroy(&queue);
  exit(0);
}

int main(int argc, char **argv) {
  int port = SERVER_PORT;
  int min_threads = THREAD_MIN;
  int max_threads = THREAD_MAX;
  const char *root = "./www";

  int opt;
  while ((opt = getopt(argc, argv, "p:t:m:d:")) != -1) {
    switch (opt) {
    case 'p':
      port = atoi(optarg);
      break;
    case 't':
      min_threads = atoi(optarg);
      break;
    case 'm':
      max_threads = atoi(optarg);
      break;
    case 'd':
      root = optarg;
      break;
    default:
      fprintf(
          stderr,
          "Usage: %s [-p port] [-t min_threads] [-m max_threads] [-d root]\n",
          argv[0]);
      return 1;
    }
  }

  if (chdir(root) < 0) {
    log_error("Cannot change to directory %s", root);
    return 1;
  }

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  if (!queue_init(&queue, QUEUE_CAPACITY)) {
    log_error("Failed to initialize queue");
    return 1;
  }

  if (!pool_init(&pool, &queue, min_threads, max_threads)) {
    log_error("Failed to initialize thread pool");
    queue_destroy(&queue);
    return 1;
  }

  server_fd = server_create(port);
  if (server_fd < 0) {
    pool_shutdown(&pool);
    queue_destroy(&queue);
    return 1;
  }

  log_info("Server ready. Press Ctrl+C to stop.");

  while (1) {
    struct sockaddr_in client_addr;
    int client_fd = server_accept(server_fd, &client_addr);

    if (client_fd < 0) {
      if (errno == EINTR)
        continue;
      usleep(1000);
      continue;
    }

    job_t job = {.client_fd = client_fd,
                 .enqueue_time = time_ms(),
                 .keep_alive = true,
                 .timeout_ms = KEEPALIVE_TIMEOUT_MS};
    pool_submit(&pool, job);
  }

  return 0;
}
