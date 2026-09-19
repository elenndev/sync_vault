#include "utils/string_utils.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool string_to_timestamp(const char *str, time_t *timestamp) {
  if (str == NULL || timestamp == NULL) {
    return false;
  }

  char *endptr;
  long long value = strtoll(str, &endptr, 10);

  if (endptr == str || *endptr != '\0') {
    return false;
  }

  *timestamp = (time_t)value;
  return true;
}

bool string_date_to_timestamp(const char *date_str, time_t *timestamp) {
  if (date_str == NULL || timestamp == NULL) {
    return false;
  }

  struct tm tm = {0};

  if (sscanf(date_str, "%d-%d-%d %d:%d:%d", &tm.tm_year, &tm.tm_mon,
             &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
    return false;
  }

  tm.tm_year -= 1900;
  tm.tm_mon -= 1;
  tm.tm_isdst = -1;

  *timestamp = mktime(&tm);

  return *timestamp != (time_t)-1;
}

bool timestamp_to_string(time_t timestamp, char *buffer, size_t buffer_size) {
  if (buffer == NULL || buffer_size == 0) {
    return false;
  }

  struct tm *tm = localtime(&timestamp);

  if (tm == NULL) {
    return false;
  }

  return strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm) > 0;
}

void string_trim(char *str) {
  char *start = str;

  while (*start == ' ' || *start == '\t')
    start++;

  if (start != str)
    memmove(str, start, strlen(start) + 1);

  int len = strlen(str);

  while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' ||
                     str[len - 1] == '\n' || str[len - 1] == '\r')) {
    str[--len] = '\0';
  }
}

void string_remove_quotes(char *str) {
  int len = strlen(str);

  if (len >= 2 && str[0] == '"' && str[len - 1] == '"') {
    memmove(str, str + 1, len - 2);
    str[len - 2] = '\0';
  }
}

void string_copy(char *dest, size_t dest_size, const char *src) {
  strncpy(dest, src, dest_size - 1);
  dest[dest_size - 1] = '\0';
}
