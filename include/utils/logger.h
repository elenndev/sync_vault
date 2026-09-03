#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>

typedef enum {
  LOG_LEVEL_DEBUG,
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARNING,
  LOG_LEVEL_ERROR
} LogLevel;

void logger_set_level(LogLevel level);

void log_debug(const char *fmt, ...);
void log_info(const char *fmt, ...);
void log_warning(const char *fmt, ...);
void log_error(const char *fmt, ...);

#endif
