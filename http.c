#include "http.h"
#include "config.h"
#include "io.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *get_mime_type(const char *path) {
  const char *ext = strrchr(path, '.');
  if (!ext)
    return "application/octet-stream";

  if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
    return "text/html";
  if (strcmp(ext, ".css") == 0)
    return "text/css";
  if (strcmp(ext, ".js") == 0)
    return "application/javascript";
  if (strcmp(ext, ".png") == 0)
    return "image/png";
  if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
    return "image/jpeg";
  if (strcmp(ext, ".gif") == 0)
    return "image/gif";
  if (strcmp(ext, ".txt") == 0)
    return "text/plain";

  return "application/octet-stream";
}

int http_handle_job(job_t *job) {
  http_request_t req;
  int req_count = 0;

  while (req_count < KEEPALIVE_MAX_REQ) {
    struct pollfd pfd = {.fd = job->client_fd, .events = POLLIN};
    int ready = poll(&pfd, 1, job->keep_alive ? job->timeout_ms : -1);

    if (ready < 0) {
      if (errno == EINTR)
        continue;
      break;
    }

    if (ready == 0) {
      break;
    }

    if (http_parse_request(job->client_fd, &req) < 0) {
      break;
    }

    req_count++;

    // uint64_t start_time = time_ms();
    int status_code = 200;

    if (strcmp(req.method, "GET") != 0 && strcmp(req.method, "HEAD") != 0) {
      status_code = 405;
      http_send_response(job->client_fd, &req, status_code,
                         "Method Not Allowed");
    } else {
      char safe_path[PATH_MAX_LEN];
      if (!path_safe(".", req.path, safe_path, sizeof(safe_path))) {
        status_code = 403;
        http_send_response(job->client_fd, &req, status_code, "Forbidden");
      } else {
        int result = http_serve_file(job->client_fd, safe_path, req.keep_alive);
        if (result < 0) {
          status_code = 404;
        }
      }
    }

    // uint64_t response_time = time_ms() - start_time;
    // log_info("%s %s %d %lums", req.method, req.path, status_code,
    //          response_time);

    if (!req.keep_alive)
      break;
  }

  close(job->client_fd);
  return 0;
}

int http_parse_request(int fd, http_request_t *req) {
  char buf[BUFFER_SIZE];
  ssize_t n = read(fd, buf, sizeof(buf) - 1);
  if (n <= 0)
    return -1;
  buf[n] = '\0';

  char *line = buf;
  char *end = strstr(line, "\r\n");
  if (!end)
    return -1;
  *end = '\0';

  if (sscanf(line, "%7s %255s %15s", req->method, req->path, req->version) !=
      3) {
    return -1;
  }

  req->keep_alive = (strcmp(req->version, "HTTP/1.1") == 0);
  req->content_length = 0;

  line = end + 2;
  while ((end = strstr(line, "\r\n")) != NULL && line != end) {
    *end = '\0';

    if (strncasecmp(line, "Connection:", 11) == 0) {
      char *val = line + 11;
      while (*val == ' ')
        val++;
      if (strcasecmp(val, "close") == 0)
        req->keep_alive = false;
      if (strcasecmp(val, "keep-alive") == 0)
        req->keep_alive = true;
    } else if (strncasecmp(line, "Content-Length:", 15) == 0) {
      req->content_length = atoi(line + 15);
    }

    line = end + 2;
  }

  return 0;
}

int http_send_response(int fd, http_request_t *req, int status,
                       const char *msg) {
  char buf[BUFFER_SIZE];
  int n = snprintf(buf, sizeof(buf),
                   "HTTP/1.1 %d %s\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: %zu\r\n"
                   "Connection: %s\r\n"
                   "\r\n"
                   "%s",
                   status, msg, strlen(msg),
                   req->keep_alive ? "keep-alive" : "close", msg);

  return io_send_buffer(fd, buf, n);
}

int http_serve_file(int fd, const char *path, bool keep_alive) {

  int file_fd = open(path, O_RDONLY);
  if (file_fd < 0) {
    http_request_t dummy = {.keep_alive = keep_alive};
    http_send_response(fd, &dummy, 404, "Not Found");
    return -1;
  }

  struct stat st;
  if (fstat(file_fd, &st) < 0) {
    close(file_fd);
    http_request_t dummy = {.keep_alive = keep_alive};
    http_send_response(fd, &dummy, 500, "Internal Server Error");
    return -1;
  }

  if (S_ISDIR(st.st_mode)) {
    close(file_fd);
    char index_path[PATH_MAX_LEN];
    snprintf(index_path, sizeof(index_path), "%s/index.html", path);
    return http_serve_file(fd, index_path, keep_alive);
  }

  char header[BUFFER_SIZE];
  int header_len = snprintf(header, sizeof(header),
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: %s\r\n"
                            "Content-Length: %ld\r\n"
                            "Connection: %s\r\n"
                            "\r\n",
                            get_mime_type(path), st.st_size,
                            keep_alive ? "keep-alive" : "close");

  io_send_buffer(fd, header, header_len);
  io_send_file(fd, file_fd, 0, st.st_size);

  close(file_fd);
  return 0;
}
