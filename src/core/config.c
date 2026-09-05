#include "core/config.h"
#include "core/config_vault.h"
#include "utils/string_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void config_set_defaults(Config *cfg) {
  strcpy(cfg->provider, "filesystem");
  strcpy(cfg->compression, "zip");
}

static void get_config_dir(char *buffer, size_t size) {
  const char *xdg = getenv("XDG_CONFIG_HOME");

  if (xdg) {
    snprintf(buffer, size, "%s/sync-vault", xdg);
  } else {
    const char *home = getenv("HOME");

    snprintf(buffer, size, "%s/.config/sync-vault", home);
  }
}

static void get_config_file(char *buffer, size_t size) {
  char dir[512];

  get_config_dir(dir, sizeof(dir));

  snprintf(buffer, size, "%s/config.conf", dir);
}

static bool ensure_config_dir(void) {
  char dir[512];

  get_config_dir(dir, sizeof(dir));

  struct stat st;

  if (stat(dir, &st) == 0) {
    return true;
  }

  return mkdir(dir, 0755) == 0;
}

bool config_save(Config *config) {
  char path[512];

  get_config_file(path, sizeof(path));

  FILE *file = fopen(path, "w");

  if (!file)
    return false;

  fprintf(file,
          "provider = \"%s\"\n"
          "compression = \"%s\"\n",
          config->provider, config->compression);

  fclose(file);

  return true;
}

bool config_load(Config *config) {
  config_set_defaults(config);

  if (!ensure_config_dir()) {
    return false;
  }

  char path[512];
  get_config_file(path, sizeof(path));

  FILE *file = fopen(path, "r");

  if (!file) {
    return config_save(config);
  }

  char line[1024];

  VaultConfig current_vault;
  memset(&current_vault, 0, sizeof(current_vault));

  bool reading_vault = false;

  while (fgets(line, sizeof(line), file)) {
    string_trim(line);

    if (strlen(line) == 0) {
      continue;
    }

    if (strcmp(line, "[[vault]]") == 0) {

      memset(&current_vault, 0, sizeof(current_vault));
      reading_vault = true;

      continue;
    }

    char *equal = strchr(line, '=');

    if (!equal) {
      continue;
    }

    *equal = '\0';

    char *key = line;
    char *value = equal + 1;

    string_trim(key);
    string_trim(value);

    string_remove_quotes(value);
    if (reading_vault) {

      if (strcmp(key, "id") == 0) {

        string_copy(current_vault.name, sizeof(current_vault.name), value);
      } else if (strcmp(key, "path") == 0) {

        string_copy(current_vault.path, sizeof(current_vault.path), value);
      }

    } else {

      if (strcmp(key, "provider") == 0) {

        string_copy(config->provider, sizeof(config->provider), value);
      } else if (strcmp(key, "compression") == 0) {

        string_copy(config->compression, sizeof(config->compression), value);
      }
    }
  }

  fclose(file);

  return true;
}
