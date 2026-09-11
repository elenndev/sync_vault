#ifndef FILES_H
#define FILES_H

#include <stdbool.h>
#include <time.h>

#define ARCHIVE_EXECUTABLE "crip-crypt"
#define ARCHIVE_FILE_NAME "vault.tar.gz.age"
#define ARCHIVE_NAME_PREFIX "vault_"
#define ARCHIVE_NAME_EXTENSION ".tar.gz.age"
#define ARCHIVE_NAME_MAX 128

typedef struct {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
} ArchiveTimestamp;

// bool archive_generate_name(char *buffer, size_t size);
//
// bool archive_parse_name(const char *filename, ArchiveTimestamp *out);
//
// bool archive_is_valid_name(const char *filename);
//
// time_t archive_timestamp_to_time_t(const ArchiveTimestamp *ts);
//
// void archive_format_timestamp(const ArchiveTimestamp *ts, char *buffer,
//                               size_t size);

bool archive_create(const char *vault_path, const char *output_directory,
                    const char *password);

bool archive_extract(const char *archive_path, const char *output_directory,
                     const char *password);

#endif
