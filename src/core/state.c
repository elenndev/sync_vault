#include "core/state.h"
#include "utils/string_utils.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static State g_state;

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

void compare_syncs(time_t last_local_sync_timestamp, time_t backup_timestamp,
                   SyncAction *action) {
  if (last_local_sync_timestamp > backup_timestamp) {
    *action = SYNC_UPLOAD;
  }

  if (last_local_sync_timestamp < backup_timestamp) {
    *action = SYNC_DOWNLOAD;
  }
}

void get_state_dir(char *buffer, size_t size) {
  const char *xdg = getenv("XDG_STATE_HOME");

  if (xdg) {
    snprintf(buffer, size, "%s/sync-vault", xdg);
  } else {
    const char *home = getenv("HOME");
    snprintf(buffer, size, "%s/.local/state/sync-vault", home);
  }
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
    printf("run auth command\n");
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
      if (strcmp(value, "0") == 0) {
        g_state.last_sync = 0;
      } else {
        if (!string_to_timestamp(value, &g_state.last_sync)) {
          fprintf(stderr, "Warning: failed to parse last sync timestamp: %s\n",
                  value);
          g_state.last_sync = 0;
        }
      }

    } else if (strcmp(key, "last_backup_timestamp") == 0) {
      if (!string_to_timestamp(value, &g_state.last_backup_timestamp)) {
        fprintf(stderr, "Warning: failed to parse last_backup_timestamp: %s\n",
                value);
        g_state.last_backup_timestamp = 0;
      }
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

  fprintf(file, "last_sync=%ld\n", (long)g_state.last_sync);
  fprintf(file, "last_backup_timestamp=%ld\n",
          (long)g_state.last_backup_timestamp);
  fprintf(file, "google_refresh_token=%s\n", g_state.google_refresh_token);
  fprintf(file, "google_access_token=%s\n", g_state.google_access_token);

  fclose(file);
  return true;
}

State *state_get(void) { return &g_state; }
