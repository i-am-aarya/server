#include "io.h"
#include <errno.h>
#include <poll.h>
#include <sys/sendfile.h>
#include <unistd.h>

ssize_t io_send_file(int out_fd, int in_fd, off_t offset, size_t count) {
  off_t off = offset;
  ssize_t total = 0;

  while (count > 0) {
    ssize_t sent = sendfile(out_fd, in_fd, &off, count);

    if (sent < 0) {
      if (errno == EINTR)
        continue;
      if (errno == EAGAIN) {
        struct pollfd pfd = {.fd = out_fd, .events = POLLOUT};
        poll(&pfd, 1, 5000);
        continue;
      }
      return total > 0 ? total : -1;
    }

    if (sent == 0)
      break; // EOF

    total += sent;
    count -= sent;
  }

  return total;
}

ssize_t io_send_buffer(int fd, const void *buf, size_t count) {
  const char *p = buf;
  ssize_t total = 0;

  while (count > 0) {
    ssize_t n = write(fd, p, count);

    if (n < 0) {
      if (errno == EINTR)
        continue;
      if (errno == EAGAIN) {
        // FIX #3: Also handle EAGAIN here
        struct pollfd pfd = {.fd = fd, .events = POLLOUT};
        poll(&pfd, 1, 5000);
        continue;
      }
      return total > 0 ? total : -1;
    }

    p += n;
    total += n;
    count -= n;
  }

  return total;
}
