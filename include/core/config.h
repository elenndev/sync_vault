#ifndef CONFIG_H
#define CONFIG_H

#include "providers/google_drive.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
  char provider[32];
  char compression[32];
} Config;

bool load_config_from_file(const char *config_path,
                           GoogleDriveProvider *provider);

bool load_credentials(GoogleDriveProvider *provider);

bool config_load(Config *config);

bool config_save(Config *config);

bool config_add_vault(Config *config, const char *name, const char *path);

#endif
