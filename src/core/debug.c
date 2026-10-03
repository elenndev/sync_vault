#include "core/debug.h"
#include <stdarg.h>
#include <stdio.h>

static bool s_debug_enabled = false;

void debug_set_enabled(bool enabled) { s_debug_enabled = enabled; }

bool debug_is_enabled(void) { return s_debug_enabled; }

void debug_log(const char *fmt, ...) {
  if (!s_debug_enabled) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  fputs("[debug] ", stderr);
  vfprintf(stderr, fmt, args);
  fputc('\n', stderr);
  va_end(args);
}
