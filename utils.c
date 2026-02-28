#include "utils.h"
#include "config.h"
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_info(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  pthread_mutex_lock(&log_mutex);
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  struct tm tm_info;
  localtime_r(&ts.tv_sec, &tm_info);
  char buf[LOG_BUF_SIZE];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_info);
  printf("[%s.%03ld] ", buf, ts.tv_nsec / 1000000);
  vprintf(fmt, args);
  printf("\n");
  fflush(stdout);
  pthread_mutex_unlock(&log_mutex);
  va_end(args);
}

void log_error(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  pthread_mutex_lock(&log_mutex);
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  struct tm tm_info;
  localtime_r(&ts.tv_sec, &tm_info);
  char buf[LOG_BUF_SIZE];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_info);
  fprintf(stderr, "[%s.%03ld] ERROR: ", buf, ts.tv_nsec / 1000000);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  fflush(stderr);
  pthread_mutex_unlock(&log_mutex);
  va_end(args);
}

uint64_t time_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

bool path_safe(const char *root, const char *requested, char *out,
               size_t out_len) {
  // Get absolute path of root (current directory is www)
  char root_abs[PATH_MAX];
  if (!realpath(root, root_abs)) {
    return false;
  }
  size_t root_len = strlen(root_abs);

  // Build requested path - remove leading slash if present
  const char *req_clean = requested;
  while (*req_clean == '/')
    req_clean++;

  char req_path[PATH_MAX + 1];
  int n = snprintf(req_path, sizeof(req_path), "%s/%s", root_abs, req_clean);

  if (n < 0 || (size_t)n >= sizeof(req_path)) {
    return false;
  }

  // Resolve the requested path
  char *resolved = realpath(req_path, NULL);

  // If file doesn't exist, check parent directory
  if (!resolved) {
    char parent[PATH_MAX];
    if (strlen(req_path) >= sizeof(parent)) {
      return false;
    }
    strcpy(parent, req_path);

    // Remove trailing slashes
    size_t len = strlen(parent);
    while (len > 0 && parent[len - 1] == '/') {
      parent[--len] = '\0';
    }

    char *last_slash = strrchr(parent, '/');
    if (!last_slash)
      return false;

    *last_slash = '\0';

    char *parent_resolved = realpath(parent, NULL);
    if (!parent_resolved)
      return false;

    bool parent_safe =
        strncmp(parent_resolved, root_abs, root_len) == 0 &&
        (parent_resolved[root_len] == '/' || parent_resolved[root_len] == '\0');

    free(parent_resolved);

    if (!parent_safe) {
      return false;
    }

    if (out) {
      strncpy(out, req_path, out_len - 1);
      out[out_len - 1] = '\0';
    }
    return true;
  }

  // Check resolved path is within root
  bool safe = strncmp(resolved, root_abs, root_len) == 0 &&
              (resolved[root_len] == '/' || resolved[root_len] == '\0');

  if (safe && out) {
    strncpy(out, resolved, out_len - 1);
    out[out_len - 1] = '\0';
  }

  free(resolved);
  return safe;
}
