#include "core/cache.h"
#include "core/cli.h"
#include "core/config.h"
#include "core/state.h"
#include "files/files.h"
#include "providers/google_drive.h"
#include "utils/colors.h"
#include "utils/files.h"
#include "utils/string_utils.h"
#include "utils/time_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <command>\n", argv[0]);
    printf("\nAvailable commands:\n");
    printf("  auth          - Authenticate with Google Drive\n");
    printf("  start         - Start the sync service\n");
    printf("  status        - Show current status\n");
    printf("  config        - Show config file location\n");
    printf("\nExamples:\n");
    printf("  %s auth\n", argv[0]);
    printf("  %s status\n", argv[0]);
    return 1;
  }

  switch (state_load()) {
  case STATE_LOAD_OK:
    break;

  case STATE_LOAD_NOT_FOUND_AUTH_NEED:
    if (strcmp(argv[1], "auth") != 0) {
      printf(COLOR_RED "Not authenticated yet. Run the 'auth' command to get "
                       "started.\n" STYLE_RESET);
      printf("run: \n%s auth\n", argv[0]);

      return 0;
    }
    break;

  case STATE_LOAD_ERR_DIR:
    printf(COLOR_RED "Failed to load state\n" STYLE_RESET);
    return 0;
    break;
  }

  if (!cache_init()) {
    fprintf(stderr, "Error: Failed to start cache\n");
    return 1;
  }

  if (strcmp(argv[1], "config") == 0) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    char config_path[512];

    if (xdg) {
      snprintf(config_path, sizeof(config_path), "%s/sync-vault/config.json",
               xdg);
    } else {
      const char *home = getenv("HOME");
      snprintf(config_path, sizeof(config_path),
               "%s/.config/sync-vault/config.json", home);
    }

    printf("\n Config file location:\n");
    printf("   %s\n\n", config_path);
    printf("Example content:\n");
    printf("{\n");
    printf("  \"google_drive\": {\n");
    printf(
        "    \"client_id\": \"YOUR_CLIENT_ID.apps.googleusercontent.com\",\n");
    printf("    \"client_secret\": \"YOUR_CLIENT_SECRET\",\n");
    printf("    \"folder_id\": \"OPTIONAL_FOLDER_ID\"\n");
    printf("  }\n");
    printf("}\n\n");
    return 0;
  }

  if (strcmp(argv[1], "auth") == 0) {
    printf("\nStarting Google Drive authentication...\n");

    GoogleDriveProvider provider = {0};

    if (!load_credentials(&provider)) {
      return 1;
    }

    if (google_drive_authenticate(&provider)) {
      printf("\nAuthentication successful!\n");
      printf("   Access token: %s...\n", provider.access_token + 10);
      return 0;
    } else {
      printf("\nAuthentication failed!\n");
      return 1;
    }
  }

  if (strcmp(argv[1], "status") == 0) {
    State *state = state_get();

    printf(STYLE_BOLD "Status\n" STYLE_RESET);

    printf("  Last sync: ");
    if (state->last_sync) {
      char sync_date[32];
      if (timestamp_to_string(state->last_sync, sync_date, sizeof(sync_date))) {
        printf(COLOR_GREEN "%s\n" STYLE_RESET, sync_date);
      }

    } else {
      printf(COLOR_RED "(never)\n" STYLE_RESET);
    }

    printf("  Last backup: ");
    if (state->last_backup_timestamp) {
      char backup_date[32];

      if (timestamp_to_string(state->last_backup_timestamp, backup_date,
                              sizeof(backup_date))) {
        printf(COLOR_GREEN "%s\n" STYLE_RESET, backup_date);
      }

    } else {
      printf(COLOR_RED "(never)\n" STYLE_RESET);
    }

    if (state->google_access_token[0]) {
      printf("  Google Drive: Authenticated\n");
    } else {
      printf("  Google Drive: Not authenticated (run 'auth')\n");
    }
    return 0;
  }

  if (strcmp(argv[1], "start") == 0) {
    SyncAction action = SYNC_NONE;
    GoogleDriveProvider provider = {0};
    State *state = state_get();

    strncpy(provider.access_token, state->google_access_token,
            sizeof(provider.access_token) - 1);
    provider.access_token[sizeof(provider.access_token) - 1] = '\0';

    strncpy(provider.refresh_token, state->google_refresh_token,
            sizeof(provider.refresh_token) - 1);
    provider.refresh_token[sizeof(provider.refresh_token) - 1] = '\0';

    if (!load_credentials(&provider)) {
      return 1;
    }

    if (!google_drive_refresh_access_token(&provider)) {
      return 1;
    }

    if (!load_vault_config(state)) {
      return 1;
    }

    const char *cache = cache_get_path();
    LatestArchive latest_archive = {0};
    list_folder_files(state->google_access_token, provider.folder_id,
                      &latest_archive);
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
    if (latest_archive.file_id[0]) {
      char date[32];
      timestamp_to_string(latest_archive.timestamp, date, sizeof(date));

      printf(COLOR_GREEN "%s\n" STYLE_RESET, date);
    } else {
      printf(COLOR_RED "(never)\n" STYLE_RESET);
    }

    if (state->google_access_token[0]) {
      printf("  Google Drive: Authenticated\n");
    } else {
      printf("  Google Drive: Not authenticated (run 'auth')\n");
    }

    char encrypted_dir[512];
    if (!ensure_encrypted_dir(cache, encrypted_dir, sizeof(encrypted_dir))) {
      return false;
    }

    if (state->last_sync == '\0') {
      action = SYNC_DOWNLOAD;
    }

    time_t now = time(NULL);

    if (state->last_sync != '\0' && is_same_day(state->last_sync, now)) {

      char last_sync_date[32];
      timestamp_to_hour(state->last_sync, last_sync_date,
                        sizeof(last_sync_date));

      char prompt[256];
      snprintf(prompt, sizeof(prompt),
               "Last sync was today at %s. Run another sync anyway?",
               last_sync_date);

      if (!get_user_confirm(prompt)) {
        printf("Skipping sync.\n");
        return 0;
      }
    }

    char password[256];
    if (!get_password(password, sizeof(password))) {
      fprintf(stderr, "Error: failed to read password\n");
      return 1;
    }

    if (action == SYNC_NONE) {
      compare_syncs(state->last_sync, latest_archive.timestamp, &action);
    }

    if (action == SYNC_DOWNLOAD) {
      printf(STYLE_BOLD "running 'sync', Downloading...\n" STYLE_RESET);

      download_file(provider.access_token, latest_archive.file_id,
                    latest_archive.file_name, encrypted_dir);

      char latest_archive_path[512];
      snprintf(latest_archive_path, sizeof(latest_archive_path), "%s/%s",
               encrypted_dir, latest_archive.file_name);

      if (!archive_extract(latest_archive_path, state->vault_path, password)) {
        archive_cleanup(latest_archive_path);
        fprintf(stderr,
                "Error: failed to decrypt and extract vault encrypted file\n");
        return 1;
      }

      state->last_sync = now;
      state->last_backup_timestamp = latest_archive.timestamp;
      state_save();
    }

    if (action == SYNC_UPLOAD) {

      if (is_directory_empty(state->vault_path)) {
        printf(STYLE_BOLD
               "Empty vault folder, cancelling upload...\n" STYLE_RESET);
        return 0;
      }
      printf(STYLE_BOLD "running 'sync' - Uploading...\n" STYLE_RESET);

      char archive_path[1024];
      char archive_name[ARCHIVE_NAME_MAX];

      if (!archive_create(state->vault_path, encrypted_dir, archive_path,
                          archive_name, password)) {
        fprintf(stderr, "Error: failed to create vault encrypted file\n");
        return 1;
      }

      if (!upload_file(state->google_access_token, provider.folder_id,
                       archive_path, archive_name)) {
        fprintf(stderr, "Error: failed upload archive\n");
        return 1;
      }

      time_t now = time(NULL);
      state->last_backup_timestamp = now;
      state->last_sync = now;

      state_save();
      archive_cleanup(archive_path);
    }

    return 0;
  }

  if (strcmp(argv[1], "sync") == 0) {
    printf("started sync...");

    GoogleDriveProvider provider = {0};
    State *state = state_get();

    strncpy(provider.access_token, state->google_access_token,
            sizeof(provider.access_token) - 1);
    provider.access_token[sizeof(provider.access_token) - 1] = '\0';

    strncpy(provider.refresh_token, state->google_refresh_token,
            sizeof(provider.refresh_token) - 1);
    provider.refresh_token[sizeof(provider.refresh_token) - 1] = '\0';

    if (!load_credentials(&provider)) {
      return 1;
    }

    if (!google_drive_refresh_access_token(&provider)) {
      return 1;
    }

    if (!load_vault_config(state)) {
      return 1;
    }

    char password[256];
    if (!get_password(password, sizeof(password))) {
      fprintf(stderr, "Error: failed to read password\n");
      return 1;
    }
    return 0;
  }

  printf("Unknown command: %s\n", argv[1]);
  printf("\nAvailable commands:\n");
  printf("  auth          - Authenticate with Google Drive\n");
  printf("  start         - Start the sync service\n");
  printf("  status        - Show current status\n");
  printf("  config        - Show config file location\n");
  return 1;
}
