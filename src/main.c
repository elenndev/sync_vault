#include "core/config.h"
#include "core/state.h"
#include "providers/google_drive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
  // Carrega o estado persistente
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
    printf("\nStatus:\n");
    printf("  Last sync: %s\n",
           state->last_sync[0] ? state->last_sync : "(never)");
    printf("  Last backup: %s\n",
           state->last_backup_name[0] ? state->last_backup_name : "(never)");

    if (state->google_access_token[0]) {
      printf("  Google Drive: Authenticated\n");
      printf("  Access token: %s...\n", state->google_access_token + 10);
    } else {
      printf("  Google Drive: Not authenticated (run 'auth')\n");
    }
    return 0;
  }

  else if (strcmp(argv[1], "start") == 0) {
    printf("started\n");
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
