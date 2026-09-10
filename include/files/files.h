#ifndef FILES_H
#define FILES_H

#include <stdbool.h>

bool archive_create(const char *vault_path, const char *output_directory,
                    const char *password);

bool archive_extract(const char *archive_path, const char *output_directory,
                     const char *password);

#endif
