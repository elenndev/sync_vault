#ifndef DEBUG_H
#define DEBUG_H

#include <stdbool.h>

void debug_set_enabled(bool enabled);

bool debug_is_enabled(void);

void debug_log(const char *fmt, ...);

#endif
