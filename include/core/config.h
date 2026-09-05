#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  char provider[32];
  char compression[32];
} Config;

bool config_load(Config *config);

bool config_save(Config *config);

bool config_add_vault(Config *config, const char *name, const char *path);

#endif
