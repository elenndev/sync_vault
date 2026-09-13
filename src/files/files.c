#include "files/files.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
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

bool archive_cleanup(const char *archive_path) {
  if (!archive_path || archive_path[0] == '\0') {
    return false;
  }

  if (unlink(archive_path) != 0) {
    if (errno == ENOENT) {
      return true;
    }
    fprintf(stderr, "Failed to remove temp file: %s (%s)\n", archive_path,
            strerror(errno));
    return false;
  }

  return true;
}

bool ensure_encrypted_dir(const char *cache_path, char *out_path,
                          size_t out_size) {
  snprintf(out_path, out_size, "%s/encrypted", cache_path);

  if (mkdir(out_path, 0755) != 0) {
    struct stat st;
    if (stat(out_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
      fprintf(stderr, "Failed to create directory: %s\n", out_path);
      return false;
    }
  }

  return true;
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

bool archive_parse_name(const char *filename, ArchiveTimestamp *out) {
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
                    char *archive_path, char *archive_name,
                    const char *password) {

  if (!archive_generate_name(archive_name, ARCHIVE_NAME_MAX)) {
    fprintf(stderr, "Failed to generate archive name \n");
    return false;
  }

  snprintf(archive_path, ARCHIVE_NAME_MAX, "%s/%s", output_directory,
           archive_name);

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
