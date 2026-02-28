#include "server.h"
#include "utils.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int server_create(int port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    log_error("socket: %s", strerror(errno));
    return -1;
  }

  int opt = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    log_error("setsockopt SO_REUSEADDR: %s", strerror(errno));
    close(fd);
    return -1;
  }

  if (setsockopt(fd, IPPROTO_TCP, TCP_CORK, &opt, sizeof(opt)) < 0) {
    log_error("setsockopt TCP_CORK: %s", strerror(errno));
    // Non-fatal
  }

  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_addr.s_addr = INADDR_ANY,
                             .sin_port = htons(port)};

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    log_error("bind: %s", strerror(errno));
    close(fd);
    return -1;
  }

  if (listen(fd, 128) < 0) {
    log_error("listen: %s", strerror(errno));
    close(fd);
    return -1;
  }

  log_info("Server listening on port %d", port);
  return fd;
}

int server_accept(int server_fd, struct sockaddr_in *client_addr) {
  socklen_t addr_len = sizeof(*client_addr);
  int fd = accept(server_fd, (struct sockaddr *)client_addr, &addr_len);

  if (fd < 0) {
    if (errno != EINTR && errno != EAGAIN) {
      log_error("accept: %s", strerror(errno));
    }
    return -1;
  }

  // Set non-blocking
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);

  // Set TCP_CORK on client socket (not server socket)
  int opt = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_CORK, &opt, sizeof(opt));

  return fd;
}
