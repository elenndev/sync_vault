#ifndef CONFIGVAULT_H
#define CONFIGVAULT_H

#define CONFIG_PATH_MAX 512

typedef struct {
  char name[64];
  char path[CONFIG_PATH_MAX];
} VaultConfig;

VaultConfig vault_create(const char *name, const char *path);

#endif
