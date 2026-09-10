#include "core/cache.h"
#include "core/config.h"
#include "core/state.h"
#include "providers/google_drive.h"
#include "utils/colors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  if (!state_load()) {
    fprintf(stderr, "Error: Failed to load state\n");
    return 1;
  }

  if (!cache_init()) {
    fprintf(stderr, "Error: Failed to start cache\n");
    return 1;
  }

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
    if (state->last_sync[0]) {
      printf(COLOR_GREEN "%s" STYLE_RESET, state->last_sync);
    } else {
      printf(COLOR_RED "(never)\n" STYLE_RESET);
    }

    printf("  Last backup: ");
    if (state->last_backup_name[0]) {
      printf(COLOR_GREEN "%s" STYLE_RESET, state->last_backup_name);
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
    printf("started\n");

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

    printf(STYLE_BOLD "Status\n" STYLE_RESET);

    printf("  Vault Path: ");
    if (state->vault_path[0]) {
      printf("%s\n", state->vault_path);
    } else {
      printf(COLOR_RED "(null\n" STYLE_RESET);
    }

    printf("  Last sync: ");
    if (state->last_sync[0]) {
      printf(COLOR_GREEN "%s" STYLE_RESET, state->last_sync);
    } else {
      printf(COLOR_RED "(never)\n" STYLE_RESET);
    }

    printf("  Last backup: ");
    if (state->last_backup_name[0]) {
      printf(COLOR_GREEN "%s" STYLE_RESET, state->last_backup_name);
    } else {
      printf(COLOR_RED "(never)\n" STYLE_RESET);
    }

    if (state->google_access_token[0]) {
      printf("  Google Drive: Authenticated\n");
    } else {
      printf("  Google Drive: Not authenticated (run 'auth')\n");
    }

    if (list_folder_files(state->google_access_token, provider.folder_id)) {
      // download_file(provider.access_token, char *file_id, char *file_name);

    } else {
      // upload file
      printf(STYLE_BOLD "run 'sync'\n" STYLE_RESET);
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
  }

  printf("Unknown command: %s\n", argv[1]);
  printf("\nAvailable commands:\n");
  printf("  auth          - Authenticate with Google Drive\n");
  printf("  start         - Start the sync service\n");
  printf("  status        - Show current status\n");
  printf("  config        - Show config file location\n");
  return 1;
}
