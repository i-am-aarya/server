#ifndef UTILS_H
#define UTILS_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h> // ADD THIS
#include <stdint.h>

void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);

uint64_t time_ms(void);
bool path_safe(const char *root, const char *requested, char *out,
               size_t out_len);

#endif
