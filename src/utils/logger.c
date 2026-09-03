#include "utils/logger.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static LogLevel current_level = LOG_LEVEL_DEBUG;

void logger_set_level(LogLevel level) { current_level = level; }

static const char *level_to_string(LogLevel level) {
  switch (level) {
  case LOG_LEVEL_DEBUG:
    return "DEBUG";

  case LOG_LEVEL_INFO:
    return "INFO";

  case LOG_LEVEL_WARNING:
    return "WARNING";

  case LOG_LEVEL_ERROR:
    return "ERROR";

  default:
    return "UNKNOWN";
  }
}

static void logger_log(LogLevel level, const char *fmt, va_list args) {
  if (level < current_level)
    return;

  time_t now = time(NULL);
  struct tm *tm = localtime(&now);

  char time_buffer[20];

  strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", tm);

  printf("[%s] [%s] ", time_buffer, level_to_string(level));

  vprintf(fmt, args);

  printf("\n");
}

void log_debug(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  logger_log(LOG_LEVEL_DEBUG, fmt, args);

  va_end(args);
}

void log_info(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  logger_log(LOG_LEVEL_INFO, fmt, args);

  va_end(args);
}

void log_warning(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  logger_log(LOG_LEVEL_WARNING, fmt, args);

  va_end(args);
}

void log_error(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  logger_log(LOG_LEVEL_ERROR, fmt, args);

  va_end(args);
}
