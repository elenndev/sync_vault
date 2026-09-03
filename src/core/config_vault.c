#include "core/config_vault.h"
#include "utils/string_utils.h"

VaultConfig vault_create(const char *name, const char *path) {
  VaultConfig vault;

  string_copy(vault.name, sizeof(vault.name), name);

  string_copy(vault.path, sizeof(vault.path), path);
  string_trim(vault.name);
  string_trim(vault.path);

  return vault;
}
