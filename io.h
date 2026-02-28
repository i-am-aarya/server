#ifndef IO_H
#define IO_H

#include <stddef.h>
#include <sys/types.h>

ssize_t io_send_file(int out_fd, int in_fd, off_t offset, size_t count);
ssize_t io_send_buffer(int fd, const void *buf, size_t count);

#endif
