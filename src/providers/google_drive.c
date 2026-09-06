#include "providers/google_drive.h"
#include "core/state.h"
#include "http/http_server.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <json-c/json_tokener.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static GoogleDriveProvider *g_provider = NULL;

static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  size_t total_size = size * nmemb;

  ResponseBuffer *buffer = (ResponseBuffer *)userp;

  if (buffer->size + total_size + 1 > buffer->capacity) {
    return 0;
  }

  memcpy(buffer->data + buffer->size, contents, total_size);

  buffer->size += total_size;
  buffer->data[buffer->size] = '\0';

  return total_size;
}

bool download_file(char *access_token, char *file_id, char *file_name) {
  char url[2048];
  snprintf(url, sizeof(url),
           "https://www.googleapis.com/drive/v3/files/%s?alt=media", file_id);
  CURL *curl = curl_easy_init();
  if (!curl) {
    return false;
  }

  char dir[512];
  get_state_dir(dir, sizeof(dir));

  char file_path[512];
  snprintf(file_path, sizeof(file_path), "%s/%s", dir, file_name);

  FILE *output_file = fopen(file_path, "wb");
  if (!output_file) {
    fprintf(stderr, "Failed to create file: %s\n", file_path);
    return false;
  }

  struct curl_slist *headers = NULL;
  char auth_header[512];
  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
           access_token);
  headers = curl_slist_append(headers, auth_header);

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, output_file);

  // curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  // curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progress_callback);

  CURLcode res = curl_easy_perform(curl);
  fclose(output_file);
  curl_easy_cleanup(curl);

  return res == CURLE_OK;
}

bool list_folder_files(char *access_token, char *folder_id) {
  char url[2048];
  snprintf(url, sizeof(url),
           "https://www.googleapis.com/drive/v3/files"
           "?q='%s'+in+parents"
           "&fields=files(id,name,mimeType,size,createdTime,modifiedTime)"
           "&pageSize=1000",
           folder_id);

  CURL *curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "Failed to initialize CURL\n");
    return false;
  }

  struct curl_slist *headers = NULL;
  char auth_header[512];
  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
           access_token);
  headers = curl_slist_append(headers, auth_header);
  headers = curl_slist_append(headers, "Content-Type: application/json");

  char response[65536] = {0};
  ResponseBuffer response_buffer = {
      .data = response, .size = 0, .capacity = sizeof(response)};

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);

  CURLcode res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    fprintf(stderr, "CURL error: %s\n", curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    return false;
  }

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);

  if (http_code != 200) {
    fprintf(stderr, "HTTP error: %ld\n", http_code);
    fprintf(stderr, "Response: %s\n", response_buffer.data);
    return false;
  }

  struct json_object *root = json_tokener_parse(response_buffer.data);
  if (!root) {
    fprintf(stderr, "Failed to parse JSON\n");
    return false;
  }

  struct json_object *files;
  if (!json_object_object_get_ex(root, "files", &files)) {
    fprintf(stderr, "No 'files' field in response\n");
    json_object_put(root);
    return false;
  }

  if (!json_object_is_type(files, json_type_array)) {
    fprintf(stderr, "'files' is not an array\n");
    json_object_put(root);
    return false;
  }

  int total_files = json_object_array_length(files);
  printf("\n📂 Found %d items in folder\n", total_files);
  printf("========================================\n\n");

  for (int i = 0; i < total_files; i++) {
    struct json_object *file = json_object_array_get_idx(files, i);
    if (!file)
      continue;

    struct json_object *name_obj, *id_obj, *mime_obj, *size_obj;

    const char *name = NULL;
    const char *file_id = NULL;
    const char *mime_type = NULL;
    double size = 0;
    bool has_size = false;

    if (json_object_object_get_ex(file, "name", &name_obj)) {
      name = json_object_get_string(name_obj);
    }

    if (json_object_object_get_ex(file, "id", &id_obj)) {
      file_id = json_object_get_string(id_obj);
    }

    if (json_object_object_get_ex(file, "mimeType", &mime_obj)) {
      mime_type = json_object_get_string(mime_obj);
    }

    if (json_object_object_get_ex(file, "size", &size_obj)) {
      size = json_object_get_double(size_obj);
      has_size = true;
    }

    if (!name || !file_id || !mime_type)
      continue;

    if (strcmp(mime_type, "application/vnd.google-apps.folder") == 0) {
      printf("%s (folder)\n", name);
      printf("ID: %s\n", file_id);
    } else {
      if (has_size) {
        if (size > 1024 * 1024) {
          printf("%s (%.2f MB)\n", name, size / (1024.0 * 1024.0));
        } else if (size > 1024) {
          printf("%s (%.2f KB)\n", name, size / 1024.0);
        } else {
          printf("%s (%0.f bytes)\n", name, size);
        }
      } else {
        if (strstr(mime_type, "application/vnd.google-apps.") != NULL) {
          printf("%s (Google Workspace)\n", name);
        } else {
          printf("%s\n", name);
        }
      }
      printf("   ID: %s\n", file_id);
      printf("   Type: %s\n", mime_type);
    }
    printf("\n");
  }

  json_object_put(root);
  return true;
}

// Response buffer callback
static size_t write_response(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  size_t total = size * nmemb;
  ResponseBuffer *response = userp;

  if (response->size + total + 1 > response->capacity) {
    return 0;
  }

  memcpy(response->data + response->size, contents, total);
  response->size += total;
  response->data[response->size] = '\0';

  return total;
}

static bool load_tokens_from_state(GoogleDriveProvider *provider) {
  State *state = state_get();
  if (!state) {
    return false;
  }

  if (state->google_access_token[0] == '\0') {
    return false;
  }

  strncpy(provider->access_token, state->google_access_token,
          sizeof(provider->access_token) - 1);
  provider->access_token[sizeof(provider->access_token) - 1] = '\0';

  strncpy(provider->refresh_token, state->google_refresh_token,
          sizeof(provider->refresh_token) - 1);
  provider->refresh_token[sizeof(provider->refresh_token) - 1] = '\0';

  printf("Tokens loaded on state\n");
  return true;
}

static bool save_tokens_to_state(GoogleDriveProvider *provider) {
  printf("=== Saving tokens to state ===\n");

  State *state = state_get();

  if (!state) {
    fprintf(stderr, "Failed to get state\n");
    return false;
  }

  strncpy(state->google_access_token, provider->access_token,
          sizeof(state->google_access_token) - 1);
  state->google_access_token[sizeof(state->google_access_token) - 1] = '\0';

  strncpy(state->google_refresh_token, provider->refresh_token,
          sizeof(state->google_refresh_token) - 1);
  state->google_refresh_token[sizeof(state->google_refresh_token) - 1] = '\0';

  printf("Calling state_save()...\n");

  if (!state_save()) {
    fprintf(stderr, "state_save() failed\n");
    return false;
  }

  printf("state_save() succeeded\n");
  printf("=== Tokens saved successfully ===\n");

  return true;
}
static bool is_token_expired(GoogleDriveProvider *provider) {
  if (provider->access_token[0] == '\0') {
    return true;
  }

  return false;
}

static void handle_oauth_callback(const char *code, const char *state) {
  if (!g_provider) {
    fprintf(stderr, "Provider not set for callback\n");
    return;
  }

  // state (CSRF)
  if (!state || !g_provider->expected_state[0]) {
    fprintf(stderr, "⚠️ State validation failed: missing state\n");
    return;
  }

  if (strcmp(state, g_provider->expected_state) != 0) {
    fprintf(stderr, "❌ State validation failed! Possible CSRF attack!\n");
    return;
  }

  printf("\n✅ State validated successfully!\n");
  printf("✅ Authorization code received: %s\n", code);

  if (!google_drive_exchange_code(g_provider, code, state)) {
    fprintf(stderr, "Failed to exchange code for tokens\n");
    return;
  }

  printf("✅ Tokens obtained successfully!\n");

  http_server_stop();
}

bool google_drive_init(GoogleDriveProvider *provider) {
  if (!provider) {
    return false;
  }

  g_provider = provider;

  if (!state_load()) {
    fprintf(stderr, "Failed to load state\n");
    return false;
  }

  if (load_tokens_from_state(provider)) {
    printf("Tokens restaured from state\n");
    return true;
  }

  printf("Token not found\n");
  return true;
}

bool google_drive_authenticate(GoogleDriveProvider *provider) {
  if (!provider) {
    return false;
  }

  g_provider = provider;

  if (load_tokens_from_state(provider)) {
    printf("Using saved tokens\n");

    if (!is_token_expired(provider)) {
      printf("Token still valid\n");
      return true;
    }

    printf("Token expired, attempting refresh...\n");
    if (google_drive_refresh_access_token(provider)) {
      printf("Refresh successful\n");
      return true;
    }

    printf("Refresh failed, re-authenticating...\n");
  }

  printf("Starting OAuth flow...\n");

  // Generate state (CSRF)
  srand(time(NULL));
  char state[64];
  snprintf(state, sizeof(state), "%d", rand());
  strncpy(provider->expected_state, state,
          sizeof(provider->expected_state) - 1);
  provider->expected_state[sizeof(provider->expected_state) - 1] = '\0';

  // Start HTTP server (callback with 2 parameters)
  if (!http_server_start(8080, handle_oauth_callback)) {
    fprintf(stderr, "Failed to start HTTP server\n");
    return false;
  }

  // Open URL in browser
  const char *redirect_uri =
      "http://localhost:8080/oauth2callback/google_drive";
  char auth_url[4096];
  snprintf(auth_url, sizeof(auth_url),
           "https://accounts.google.com/o/oauth2/v2/auth"
           "?client_id=%s"
           "&redirect_uri=%s"
           "&response_type=code"
           "&scope=https%%3A%%2F%%2Fwww.googleapis.com%%2Fauth%%2Fdrive.file"
           "&access_type=offline"
           "&prompt=consent"
           "&state=%s",
           provider->client_id, redirect_uri, state);

  char command[512];
  snprintf(command, sizeof(command), "xdg-open '%s'", auth_url);
  system(command);

  printf("🌐 Browser opened for authentication\n");
  printf("📋 Waiting for callback at %s\n", redirect_uri);

  // Wait for callback
  while (http_server_is_running()) {
    sleep(1);
  }

  // Check if token was received
  if (provider->access_token[0] == '\0') {
    fprintf(stderr, "❌ Authentication failed or was cancelled\n");
    return false;
  }

  printf("✅ Authentication completed successfully!\n");
  return true;
}
bool google_drive_refresh_access_token(GoogleDriveProvider *provider) {
  if (!provider) {
    return false;
  }

  if (provider->refresh_token[0] == '\0') {
    fprintf(stderr, "No refresh token available\n");
    return false;
  }

  CURL *curl = curl_easy_init();
  if (!curl) {
    return false;
  }

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(
      headers, "Content-Type: application/x-www-form-urlencoded");

  char post_data[4096];
  snprintf(
      post_data, sizeof(post_data),
      "client_id=%s&client_secret=%s&refresh_token=%s&grant_type=refresh_token",
      provider->client_id, provider->client_secret, provider->refresh_token);

  char response[8192] = {0};
  ResponseBuffer response_buffer = {
      .data = response, .size = 0, .capacity = sizeof(response)};

  curl_easy_setopt(curl, CURLOPT_URL, "https://oauth2.googleapis.com/token");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);

  CURLcode result = curl_easy_perform(curl);
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (result != CURLE_OK) {
    fprintf(stderr, "Curl Error: %s\n", curl_easy_strerror(result));
    return false;
  }

  if (http_code != 200) {
    fprintf(stderr, "Google drive http error: %ld\n", http_code);
    fprintf(stderr, "Response: %s\n", response);
    return false;
  }

  printf("response:\n%s\n", response);

  strncpy(provider->access_token, response, sizeof(provider->access_token) - 1);
  provider->access_token[sizeof(provider->access_token) - 1] = '\0';

  if (!save_tokens_to_state(provider)) {
    fprintf(stderr, "Failed to save refreshed tokens\n");
    return false;
  }

  return true;
}

bool google_drive_exchange_code(GoogleDriveProvider *provider, const char *code,
                                const char *state) {
  if (!provider || !code) {
    fprintf(stderr, "Invalid parameters for exchange\n");
    return false;
  }

  if (!state || !provider->expected_state[0]) {
    fprintf(stderr, "State validation failed in exchange\n");
    return false;
  }

  if (strcmp(state, provider->expected_state) != 0) {
    fprintf(stderr, "States don't match.\n");
    fprintf(stderr, "   Expected: %s\n", provider->expected_state);
    fprintf(stderr, "   Received: %s\n", state);
    return false;
  }

  CURL *curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "Failed to initialize CURL\n");
    return false;
  }

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(
      headers, "Content-Type: application/x-www-form-urlencoded");

  char redirect_uri[256];
  snprintf(redirect_uri, sizeof(redirect_uri),
           "http://localhost:8080/oauth2callback/google_drive");

  char post_data[2048];
  snprintf(post_data, sizeof(post_data),
           "code=%s&client_id=%s&client_secret=%s&redirect_uri=%s&grant_type="
           "authorization_code",
           code, provider->client_id, provider->client_secret, redirect_uri);

  char response[8192] = {0};
  ResponseBuffer response_buffer = {
      .data = response, .size = 0, .capacity = sizeof(response)};

  curl_easy_setopt(curl, CURLOPT_URL, "https://oauth2.googleapis.com/token");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);

  printf("Exchanging code for tokens...\n");

  CURLcode result = curl_easy_perform(curl);
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (result != CURLE_OK) {
    fprintf(stderr, "Curl Error: %s\n", curl_easy_strerror(result));
    return false;
  }

  if (http_code != 200) {
    fprintf(stderr, "HTTP error: %ld\n", http_code);
    fprintf(stderr, "Response: %s\n", response);
    return false;
  }

  printf("Tokens received!\n");
  printf("Response: %s\n", response);

  struct json_object *json = json_tokener_parse(response);

  if (json == NULL) {
    fprintf(stderr, "Failed to parse token response\n");
    return false;
  }

  struct json_object *access_token = NULL;
  struct json_object *refresh_token = NULL;

  if (!json_object_object_get_ex(json, "access_token", &access_token) ||
      !json_object_is_type(access_token, json_type_string)) {
    fprintf(stderr, "access_token not found in response\n");
    json_object_put(json);
    return false;
  }

  if (!json_object_object_get_ex(json, "refresh_token", &refresh_token) ||
      !json_object_is_type(refresh_token, json_type_string)) {
    fprintf(stderr, "refresh_token not found in response\n");
    json_object_put(json);
    return false;
  }

  const char *access_token_value = json_object_get_string(access_token);
  const char *refresh_token_value = json_object_get_string(refresh_token);

  strncpy(provider->access_token, access_token_value,
          sizeof(provider->access_token) - 1);
  provider->access_token[sizeof(provider->access_token) - 1] = '\0';

  strncpy(provider->refresh_token, refresh_token_value,
          sizeof(provider->refresh_token) - 1);
  provider->refresh_token[sizeof(provider->refresh_token) - 1] = '\0';

  json_object_put(json);

  if (!save_tokens_to_state(provider)) {
    fprintf(stderr, "Failed to save tokens to state\n");
    return false;
  }

  printf("Tokens saved to state\n");
  return true;
}
