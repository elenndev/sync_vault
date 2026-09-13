#ifndef FILES_H
#define FILES_H

#include <stdbool.h>

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

bool archive_create(const char *vault_path, const char *output_directory,
                    char *archive_path, char *archive_name,
                    const char *password);

bool archive_extract(const char *archive_path, const char *output_directory,
                     const char *password);

#endif
