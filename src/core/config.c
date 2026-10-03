#include "core/config.h"
#include "core/debug.h"
#include "core/state.h"
#include "providers/google_drive.h"
#include "utils/colors.h"
#include "utils/files.h"
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool load_credentials_from_file(const char *config_path,
                                       GoogleDriveProvider *provider) {
  FILE *file = fopen(config_path, "r");
  if (!file) {
    return false;
  }

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *data = malloc(size + 1);
  if (!data) {
    fclose(file);
    return false;
  }

  fread(data, 1, size, file);
  data[size] = '\0';
  fclose(file);

  struct json_object *parsed = json_tokener_parse(data);
  free(data);

  if (!parsed) {
    fprintf(stderr, "Failed to parse JSON config file\n");
    return false;
  }

  // update provider values
  struct json_object *google = json_object_object_get(parsed, "google_drive");
  if (!google) {
    fprintf(stderr, "Missing 'google_drive' object in config\n");
    json_object_put(parsed);
    return false;
  }

  struct json_object *client_id_obj =
      json_object_object_get(google, "client_id");
  if (client_id_obj &&
      json_object_get_type(client_id_obj) == json_type_string) {
    const char *client_id = json_object_get_string(client_id_obj);
    strncpy(provider->client_id, client_id, sizeof(provider->client_id) - 1);
    provider->client_id[sizeof(provider->client_id) - 1] = '\0';
  } else {
    fprintf(stderr, "Missing or invalid 'client_id' in config\n");
    json_object_put(parsed);
    return false;
  }

  struct json_object *client_secret_obj =
      json_object_object_get(google, "client_secret");
  if (client_secret_obj &&
      json_object_get_type(client_secret_obj) == json_type_string) {
    const char *client_secret = json_object_get_string(client_secret_obj);
    strncpy(provider->client_secret, client_secret,
            sizeof(provider->client_secret) - 1);
    provider->client_secret[sizeof(provider->client_secret) - 1] = '\0';
  } else {
    fprintf(stderr, "Missing or invalid 'client_secret' in config\n");
    json_object_put(parsed);
    return false;
  }

  struct json_object *folder_id_obj =
      json_object_object_get(google, "folder_id");
  if (folder_id_obj &&
      json_object_get_type(folder_id_obj) == json_type_string) {
    const char *folder_id = json_object_get_string(folder_id_obj);
    strncpy(provider->folder_id, folder_id, sizeof(provider->folder_id) - 1);
    provider->folder_id[sizeof(provider->folder_id) - 1] = '\0';
  }

  json_object_put(parsed);
  return true;
}

static bool load_vault_config_from_file(const char *config_path, State *state) {
  FILE *file = fopen(config_path, "r");
  if (!file) {
    return false;
  }

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *data = malloc(size + 1);
  if (!data) {
    fclose(file);
    return false;
  }

  fread(data, 1, size, file);
  data[size] = '\0';
  fclose(file);

  struct json_object *parsed = json_tokener_parse(data);
  free(data);

  if (!parsed) {
    fprintf(stderr, "Failed to parse JSON config file\n");
    return false;
  }

  struct json_object *vault_config = NULL;
  if (!json_object_object_get_ex(parsed, "vault_config", &vault_config)) {
    fprintf(stderr, "Missing 'vault_config' object in config\n");
    json_object_put(parsed);

    printf(STYLE_BOLD "===\nNo Vault Config found!===\n\n" STYLE_RESET);
    printf("Please set your Vault Path using one of these "
           "methods:\n\n");
    printf("  Method 1: Environment variables (recommended)\n");
    printf("    export VAULT_PATH='your_vault_path'\n");
    printf("  Method 2: Config file\n");
    printf("    Create: %s\n", config_path);
    printf("    add the content:\n");
    printf("    {\n");
    printf("      \"vault_path\": \"your vault path\",\n");
    printf("\n");

    return false;
  }

  struct json_object *path_obj = NULL;
  if (!json_object_object_get_ex(vault_config, "path", &path_obj)) {
    fprintf(stderr, COLOR_RED
            "Warning: 'path' field not found in vault_config\n" STYLE_RESET);

    return false;
  }

  const char *vault_path_str = json_object_get_string(path_obj);
  if (!vault_path_str || strlen(vault_path_str) == 0) {
    return false;
  }

  if (!directory_exists(vault_path_str)) {
    fprintf(stderr, COLOR_RED "Warning: '%s' is not a valid path\n" STYLE_RESET,
            vault_path_str);
    return false;
  }

  strncpy(state->vault_path, vault_path_str, sizeof(state->vault_path) - 1);
  state->vault_path[sizeof(state->vault_path) - 1] = '\0';
  json_object_put(parsed);
  return true;
}

bool load_credentials(GoogleDriveProvider *provider) {
  const char *env_id = getenv("GOOGLE_CLIENT_ID");
  const char *env_secret = getenv("GOOGLE_CLIENT_SECRET");

  if (env_id && env_secret) {
    strncpy(provider->client_id, env_id, sizeof(provider->client_id) - 1);
    strncpy(provider->client_secret, env_secret,
            sizeof(provider->client_secret) - 1);
    printf("Credentials loaded from environment variables\n");
    return true;
  }

  // default config file (XDG)
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

  // show only when debug
  debug_log("Looking for config file: %s\n", config_path);

  if (load_credentials_from_file(config_path, provider)) {
    debug_log("Credentials loaded from config file\n");
    return true;
  }

  printf("\nNo credentials found!\n\n");
  printf("Please set your Google Drive credentials using one of these "
         "methods:\n\n");
  printf("  Method 1: Environment variables (recommended)\n");
  printf("    export GOOGLE_CLIENT_ID='your_client_id'\n");
  printf("    export GOOGLE_CLIENT_SECRET='your_client_secret'\n\n");
  printf("  Method 2: Config file\n");
  printf("    Create: %s\n", config_path);
  printf("    With content:\n");
  printf("    {\n");
  printf("      \"google_drive\": {\n");
  printf("        \"client_id\": \"your_client_id\",\n");
  printf("        \"client_secret\": \"your_client_secret\",\n");
  printf("        \"folder_id\": \"optional_folder_id\"\n");
  printf("      }\n");
  printf("    }\n");
  printf("\n");

  return false;
}

bool load_vault_config(State *state) {
  const char *env_vault_path = getenv("VAULT_PATH");

  if (env_vault_path) {
    strncpy(state->vault_path, env_vault_path, sizeof(state->vault_path) - 1);
    printf("Vault path loaded from environment variables\n");
    return true;
  }

  // default config file (XDG)
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

  if (load_vault_config_from_file(config_path, state)) {
    debug_log("Vault config loaded from config file\n");
    return true;
  }

  return false;
}
