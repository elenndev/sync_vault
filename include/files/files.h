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

typedef struct {
  char file_id[128];
  char file_name[ARCHIVE_NAME_MAX];
  time_t timestamp;
  bool found;
} LatestArchive;

time_t archive_timestamp_to_time_t(const ArchiveTimestamp *ts);

bool archive_parse_name(const char *filename, ArchiveTimestamp *out);
bool archive_cleanup(const char *archive_path);
bool ensure_encrypted_dir(const char *cache_path, char *out_path,
                          size_t out_size);
bool archive_create(const char *vault_path, const char *output_directory,
                    char *archive_path, char *archive_name,
                    const char *password);

bool archive_extract(const char *archive_path, const char *output_directory,
                     const char *password);

#endif
