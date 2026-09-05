#include "core/config.h"
#include "core/config_vault.h"
#include "providers/google_drive.h"
#include "utils/string_utils.h"
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

bool load_config_from_file(const char *config_path,
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

  printf("Looking for config file: %s\n", config_path);

  if (load_config_from_file(config_path, provider)) {
    printf("Credentials loaded from config file\n");
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
