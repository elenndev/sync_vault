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

  else if (strcmp(argv[1], "status") == 0) {
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

  else if (strcmp(argv[1], "start") == 0) {
    printf("started\n");

    GoogleDriveProvider provider = {0};
    State *state = state_get();

    if (!load_credentials(&provider)) {
      return 1;
    }

    if (list_folder_files(state->google_access_token, provider.folder_id)) {
      // download_file(provider.access_token, char *file_id, char *file_name);

    } else {
      printf("run 'sync'\n");
    }

    return 0;
  }

  else {
    printf("Unknown command: %s\n", argv[1]);
    printf("\nAvailable commands:\n");
    printf("  auth          - Authenticate with Google Drive\n");
    printf("  start         - Start the sync service\n");
    printf("  status        - Show current status\n");
    printf("  config        - Show config file location\n");
    return 1;
  }
}
