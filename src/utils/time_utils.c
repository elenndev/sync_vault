#define _POSIX_C_SOURCE 200809L
#include "utils/time_utils.h"
#include <time.h>

bool is_same_day(time_t t1, time_t t2) {
  struct tm tm1, tm2;

  if (!localtime_r(&t1, &tm1) || !localtime_r(&t2, &tm2)) {
    return false;
  }

  return tm1.tm_year == tm2.tm_year && tm1.tm_mon == tm2.tm_mon &&
         tm1.tm_mday == tm2.tm_mday;
}

void timestamp_to_hour(time_t ts, char *buffer, size_t size) {
  if (!buffer || size == 0)
    return;

  struct tm *tm_info = localtime(&ts);
  if (!tm_info) {
    buffer[0] = '\0';
    return;
  }

  strftime(buffer, size, "%I:%M %p", tm_info);
}
