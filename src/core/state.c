#include "core/state.h"
#include "core/config.h"
#include "providers/google_drive.h"
#include "utils/colors.h"
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

StateLoadResult state_load(void) {
  if (!ensure_state_dir()) {
    return STATE_LOAD_ERR_DIR;
  }

  char path[512];
  get_state_file(path, sizeof(path));

  FILE *file = fopen(path, "r");
  if (!file) {
    memset(&g_state, 0, sizeof(g_state));
    return STATE_LOAD_NOT_FOUND_AUTH_NEED;
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
  return STATE_LOAD_OK;
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

bool status_load(State *state, GoogleDriveProvider *provider,
                 LatestArchive *latest_archive) {
  strncpy(provider->access_token, state->google_access_token,
          sizeof(provider->access_token) - 1);
  provider->access_token[sizeof(provider->access_token) - 1] = '\0';

  strncpy(provider->refresh_token, state->google_refresh_token,
          sizeof(provider->refresh_token) - 1);
  provider->refresh_token[sizeof(provider->refresh_token) - 1] = '\0';

  if (!load_credentials(provider)) {
    return false;
  }

  if (!google_drive_refresh_access_token(provider)) {
    return false;
  }

  if (!load_vault_config(state)) {
    return false;
  }

  list_folder_files(state->google_access_token, provider->folder_id,
                    latest_archive);
  state_save();

  printf(STYLE_BOLD "Status\n" STYLE_RESET);
  printf("  Vault Path: ");
  if (state->vault_path[0]) {
    printf("%s\n", state->vault_path);
  } else {
    printf(COLOR_RED "(null\n" STYLE_RESET);
  }

  printf("  Last sync: ");
  if (state->last_sync) {
    char date[32];
    timestamp_to_string(state->last_sync, date, sizeof(date));

    printf(COLOR_GREEN "%s\n" STYLE_RESET, date);
  } else {
    printf(COLOR_RED "(never)\n" STYLE_RESET);
  }

  printf("  Last backup: ");
  if (latest_archive->file_id[0]) {
    char date[32];
    timestamp_to_string(latest_archive->timestamp, date, sizeof(date));

    printf(COLOR_GREEN "%s\n" STYLE_RESET, date);
  } else {
    printf(COLOR_RED "(never)\n" STYLE_RESET);
  }

  if (state->google_access_token[0]) {
    printf("  Backup Provider: Authenticated\n");
  } else {
    printf("  Backup Provider: Not authenticated (run 'auth')\n");
  }

  return true;
}

State *state_get(void) { return &g_state; }
