#ifndef TIMEUTILS_H
#define TIMEUTILS_H

#include <stdbool.h>
#include <time.h>

bool is_same_day(time_t t1, time_t t2);
void timestamp_to_hour(time_t ts, char *buffer, size_t size);

#endif
