#include "core/state.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static State g_state;

static void get_state_dir(char *buffer, size_t size) {
  const char *xdg = getenv("XDG_STATE_HOME");

  if (xdg) {
    snprintf(buffer, size, "%s/sync-vault", xdg);
  } else {
    const char *home = getenv("HOME");
    snprintf(buffer, size, "%s/.local/state/sync-vault", home);
  }
}

static void get_state_file(char *buffer, size_t size) {
  char dir[512];
  get_state_dir(dir, sizeof(dir));
  snprintf(buffer, size, "%s/state.txt", dir);
}

static bool ensure_state_dir(void) {
  char dir[512];
  get_state_dir(dir, sizeof(dir));

  struct stat st;
  if (stat(dir, &st) == 0) {
    return S_ISDIR(st.st_mode);
  }

  return mkdir(dir, 0755) == 0;
}

bool state_load(void) {
  if (!ensure_state_dir()) {
    return false;
  }

  char path[512];
  get_state_file(path, sizeof(path));

  FILE *file = fopen(path, "r");
  if (!file) {
    memset(&g_state, 0, sizeof(g_state));
    return true;
  }

  char line[512];
  while (fgets(line, sizeof(line), file)) {
    char *separator = strchr(line, '=');
    if (!separator) {
      continue;
    }

    *separator = '\0';
    char *key = line;
    char *value = separator + 1;
    value[strcspn(value, "\r\n")] = '\0';

    if (strcmp(key, "last_sync") == 0) {
      snprintf(g_state.last_sync, sizeof(g_state.last_sync), "%s", value);
    } else if (strcmp(key, "last_backup_name") == 0) {
      snprintf(g_state.last_backup_name, sizeof(g_state.last_backup_name), "%s",
               value);
    } else if (strcmp(key, "last_backup_hash") == 0) {
      snprintf(g_state.last_backup_hash, sizeof(g_state.last_backup_hash), "%s",
               value);
    } else if (strcmp(key, "google_access_token") == 0) {
      snprintf(g_state.google_access_token, sizeof(g_state.google_access_token),
               "%s", value);
    } else if (strcmp(key, "google_refresh_token") == 0) {
      snprintf(g_state.google_refresh_token,
               sizeof(g_state.google_refresh_token), "%s", value);
    }
  }

  fclose(file);
  return true;
}

bool state_save(void) {
  if (!ensure_state_dir()) {
    return false;
  }

  char path[512];
  get_state_file(path, sizeof(path));

  FILE *file = fopen(path, "w");
  if (!file) {
    fprintf(stderr, "Failed to open state file for writing: %s\n", path);
    return false;
  }

  // Escreve todos os campos
  fprintf(file, "last_sync=%s\n", g_state.last_sync);
  fprintf(file, "last_backup_name=%s\n", g_state.last_backup_name);
  fprintf(file, "last_backup_hash=%s\n", g_state.last_backup_hash);
  fprintf(file, "google_access_token=%s\n", g_state.google_access_token);
  fprintf(file, "google_refresh_token=%s\n", g_state.google_refresh_token);

  fclose(file);
  return true;
}

State *state_get(void) { return &g_state; }
