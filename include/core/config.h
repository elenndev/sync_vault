#ifndef CONFIG_H
#define CONFIG_H

#include "core/config_vault.h"
#include "providers/google_drive.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
  GoogleDriveConfig provider;
  VaultConfig vault;
} Config;

bool load_config_from_file(const char *config_path,
                           GoogleDriveProvider *provider);

bool load_credentials(GoogleDriveProvider *provider);

bool config_add_vault(Config *config, const char *name, const char *path);

#endif
