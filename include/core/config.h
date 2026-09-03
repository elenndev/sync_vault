#ifndef CONFIG_H
#define CONFIG_H

#include "core/config_vault.h"
#include <stdbool.h>
#include <stddef.h>

#define CONFIG_MAX_VAULTS 16

typedef struct {
  char provider[32];
  char compression[32];

  VaultConfig vaults[CONFIG_MAX_VAULTS];
  size_t vault_count;
} Config;

bool config_load(Config *config);

bool config_save(Config *config);

bool config_add_vault(Config *config, const char *name, const char *path);

#endif
