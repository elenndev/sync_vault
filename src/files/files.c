#include "files/files.h"

#include <stdio.h>
#include <stdlib.h>

#define ARCHIVE_EXECUTABLE "crip-crypt"
#define ARCHIVE_FILE_NAME "vault.tar.gz.age"

bool archive_create(const char *vault_path, const char *output_directory,
                    const char *password) {
  char command[2048];

  snprintf(command, sizeof(command),
           "%s encrypt \"%s\" \"%s\" "
           "--password \"%s\" "
           "--output-name \"%s\"",
           ARCHIVE_EXECUTABLE, vault_path, output_directory, password,
           ARCHIVE_FILE_NAME);

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
