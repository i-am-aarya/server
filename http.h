#ifndef HTTP_H
#define HTTP_H

#include "queue.h"

typedef struct {
  char method[8];
  char path[256];
  char version[16];
  bool keep_alive;
  int content_length;
} http_request_t;

int http_handle_job(job_t *job);
int http_parse_request(int fd, http_request_t *req);
int http_send_response(int fd, http_request_t *req, int status,
                       const char *msg);
int http_serve_file(int fd, const char *path, bool keep_alive);

#endif
