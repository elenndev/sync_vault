#ifndef STRINGUTILS_H
#define STRINGUTILS_H

#include <stddef.h>
void string_trim(char *str);
void string_remove_quotes(char *str);
void string_copy(char *dest, size_t dest_size, const char *src);

#include <stdbool.h>
#include <time.h>

bool string_to_timestamp(const char *str, time_t *timestamp);
bool string_date_to_timestamp(const char *date_str, time_t *timestamp);
bool timestamp_to_string(time_t timestamp, char *buffer, size_t buffer_size);

#endif
