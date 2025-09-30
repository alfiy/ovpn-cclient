#ifndef LOG_UTIL_H
#define LOG_UTIL_H

#include <stdarg.h>
#include <limits.h>

char* get_ovpn_log_path(void);
void log_message(const char *level, const char *format, ...);

#endif
