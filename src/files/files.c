#include "files/files.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ARCHIVE_EXECUTABLE "crip-crypt"
#define ARCHIVE_FILE_NAME "vault.tar.gz.age"

time_t archive_timestamp_to_time_t(const ArchiveTimestamp *ts) {
  if (!ts)
    return (time_t)-1;

  struct tm tm_info = {0};
  tm_info.tm_year = ts->year - 1900;
  tm_info.tm_mon = ts->month - 1;
  tm_info.tm_mday = ts->day;
  tm_info.tm_hour = ts->hour;
  tm_info.tm_min = ts->minute;
  tm_info.tm_sec = ts->second;
  tm_info.tm_isdst = -1;

  return mktime(&tm_info);
}

static bool archive_generate_name_at(char *buffer, size_t size,
                                     time_t timestamp) {
  if (!buffer || size < ARCHIVE_NAME_MAX) {
    return false;
  }

  struct tm *tm_info = localtime(&timestamp);
  if (!tm_info) {
    return false;
  }

  int written =
      snprintf(buffer, size,
               ARCHIVE_NAME_PREFIX
               "%04d-%02d-%02d_%02d-%02d-%02d" ARCHIVE_NAME_EXTENSION,
               tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
               tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

  return written > 0 && (size_t)written < size;
}

static bool archive_generate_name(char *buffer, size_t size) {
  return archive_generate_name_at(buffer, size, time(NULL));
}

static bool archive_parse_name(const char *filename, ArchiveTimestamp *out) {
  if (!filename || !out) {
    return false;
  }

  size_t prefix_len = strlen(ARCHIVE_NAME_PREFIX);
  if (strncmp(filename, ARCHIVE_NAME_PREFIX, prefix_len) != 0) {
    return false;
  }

  size_t ext_len = strlen(ARCHIVE_NAME_EXTENSION);
  size_t name_len = strlen(filename);

  if (name_len < prefix_len + ext_len) {
    return false;
  }

  if (strcmp(filename + name_len - ext_len, ARCHIVE_NAME_EXTENSION) != 0) {
    return false;
  }

  int year, month, day, hour, minute, second;
  int parsed = sscanf(filename + prefix_len, "%4d-%2d-%2d_%2d-%2d-%2d", &year,
                      &month, &day, &hour, &minute, &second);

  if (parsed != 6) {
    return false;
  }

  if (year < 1970 || year > 9999)
    return false;
  if (month < 1 || month > 12)
    return false;
  if (day < 1 || day > 31)
    return false;
  if (hour < 0 || hour > 23)
    return false;
  if (minute < 0 || minute > 59)
    return false;
  if (second < 0 || second > 60)
    return false;

  out->year = year;
  out->month = month;
  out->day = day;
  out->hour = hour;
  out->minute = minute;
  out->second = second;

  return true;
}

static bool archive_is_valid_name(const char *filename) {
  ArchiveTimestamp ts;
  return archive_parse_name(filename, &ts);
}

static void archive_format_timestamp(const ArchiveTimestamp *ts, char *buffer,
                                     size_t size) {
  if (!ts || !buffer || size == 0)
    return;

  snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d", ts->year, ts->month,
           ts->day, ts->hour, ts->minute, ts->second);
}

bool archive_create(const char *vault_path, const char *output_directory,
                    const char *password) {
  char archive_name[ARCHIVE_NAME_MAX];

  if (!archive_generate_name(archive_name, sizeof(archive_name))) {
    fprintf(stderr, "Failed to generate archive name\n");
    return false;
  }

  char command[2048];
  snprintf(command, sizeof(command),
           "%s encrypt \"%s\" \"%s\" "
           "--password \"%s\" "
           "--output-name \"%s\"",
           ARCHIVE_EXECUTABLE, vault_path, output_directory, password,
           archive_name);

  return system(command) == 0;
}

bool archive_extract(const char *archive_path, const char *output_directory,
                     const char *password) {
  char command[2048];

  snprintf(command, sizeof(command),
           "%s decrypt \"%s\" \"%s\" --password \"%s\"", ARCHIVE_EXECUTABLE,
           archive_path, output_directory, password);

  return system(command) == 0;
}
