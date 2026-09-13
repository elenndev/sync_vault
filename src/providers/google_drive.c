#include "providers/google_drive.h"
#include "core/state.h"
#include "files/files.h"
#include "http/http_server.h"
#include "utils/colors.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <json-c/json_tokener.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

// i know this is terrible its not supose to be here etc i just need this to
// work fn
static const bool show_details = false;

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

bool download_file(char *access_token, char *file_id, char *file_name,
                   const char *cache_path) {
  printf("\n");
  printf(STYLE_BOLD "Downloading file...\n" STYLE_RESET);

  char url[2048];
  snprintf(url, sizeof(url),
           "https://www.googleapis.com/drive/v3/files/%s?alt=media", file_id);
  CURL *curl = curl_easy_init();
  if (!curl) {
    return false;
  }

  char file_path[512];
  snprintf(file_path, sizeof(file_path), "%s/%s", cache_path, file_name);

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

bool list_folder_files(char *access_token, char *folder_id,
                       LatestArchive *out_latest) {
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
  if (total_files == 0) {
    printf(COLOR_RED "There is no backup yet\n" STYLE_RESET);
    json_object_put(root);
    return false;
  }

  if (show_details) {
    printf(STYLE_BOLD "\nFound %d items in folder\n" STYLE_RESET, total_files);
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
  }

  // find latest backup
  LatestArchive latest = {0};
  latest.timestamp = 0;
  latest.found = false;

  for (int i = 0; i < total_files; i++) {
    struct json_object *file = json_object_array_get_idx(files, i);
    if (!file)
      continue;

    struct json_object *name_obj, *id_obj, *mime_obj;

    const char *name = NULL;
    const char *file_id = NULL;
    const char *mime_type = NULL;

    if (json_object_object_get_ex(file, "name", &name_obj)) {
      name = json_object_get_string(name_obj);
    }
    if (json_object_object_get_ex(file, "id", &id_obj)) {
      file_id = json_object_get_string(id_obj);
    }
    if (json_object_object_get_ex(file, "mimeType", &mime_obj)) {
      mime_type = json_object_get_string(mime_obj);
    }

    if (!name || !file_id || !mime_type)
      continue;

    // skip Google Workspace files
    if (strcmp(mime_type, "application/vnd.google-apps.folder") == 0)
      continue;
    if (strstr(mime_type, "application/vnd.google-apps.") != NULL)
      continue;

    ArchiveTimestamp ts;
    if (!archive_parse_name(name, &ts)) {
      continue; // não é um backup válido
    }

    time_t file_time = archive_timestamp_to_time_t(&ts);
    if (file_time == (time_t)-1)
      continue;

    if (!latest.found || file_time > latest.timestamp) {
      latest.timestamp = file_time;
      latest.found = true;
      strncpy(latest.file_id, file_id, sizeof(latest.file_id) - 1);
      latest.file_id[sizeof(latest.file_id) - 1] = '\0';
      strncpy(latest.file_name, name, sizeof(latest.file_name) - 1);
      latest.file_name[sizeof(latest.file_name) - 1] = '\0';
    }
  }

  // show latest backup
  if (latest.found) {
    char date_str[64];
    struct tm *tm_info = localtime(&latest.timestamp);
    strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M:%S", tm_info);

    time_t now = time(NULL);
    double seconds_ago = difftime(now, latest.timestamp);
    char ago_str[64];

    if (seconds_ago < 60) {
      snprintf(ago_str, sizeof(ago_str), "just now");
    } else if (seconds_ago < 3600) {
      snprintf(ago_str, sizeof(ago_str), "%.0f minutes ago", seconds_ago / 60);
    } else if (seconds_ago < 86400) {
      snprintf(ago_str, sizeof(ago_str), "%.0f hours ago", seconds_ago / 3600);
    } else {
      snprintf(ago_str, sizeof(ago_str), "%.0f days ago", seconds_ago / 86400);
    }

    printf("\n");
    printf(STYLE_BOLD "Latest backup:\n" STYLE_RESET);
    printf("   Date: %s\n", date_str);
    printf("   Age:  %s\n", ago_str);
    printf("   File: %s\n", latest.file_name);
    printf("\n");
  } else {
    printf(COLOR_YELLOW "No valid backups found in folder\n" STYLE_RESET);
  }

  json_object_put(root);

  if (out_latest) {
    *out_latest = latest;
  }

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

static size_t read_callback(char *ptr, size_t size, size_t nmemb,
                            void *userdata) {
  UploadFileContext *ctx = (UploadFileContext *)userdata;
  return fread(ptr, size, nmemb, ctx->file);
}

static size_t header_callback(char *buffer, size_t size, size_t nitems,
                              void *userdata) {
  size_t total = size * nitems;
  ResponseBuffer *hb = (ResponseBuffer *)userdata;

  if (hb->size + total + 1 > hb->capacity) {
    return 0;
  }

  memcpy(hb->data + hb->size, buffer, total);
  hb->size += total;
  hb->data[hb->size] = '\0';
  return total;
}

static char *extract_location_header(const char *headers) {
  if (!headers)
    return NULL;

  const char *p = headers;
  const char *prefix = "location:";
  size_t prefix_len = strlen(prefix);

  while (*p) {
    if (strncasecmp(p, prefix, prefix_len) == 0) {
      const char *value = p + prefix_len;

      while (*value == ' ' || *value == '\t')
        value++;

      const char *end = value;
      while (*end && *end != '\r' && *end != '\n')
        end++;

      size_t len = end - value;
      if (len == 0)
        return NULL;

      char *location = malloc(len + 1);
      if (!location)
        return NULL;

      memcpy(location, value, len);
      location[len] = '\0';
      return location;
    }

    const char *nl = strchr(p, '\n');
    if (!nl)
      break;
    p = nl + 1;
  }

  return NULL;
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

bool upload_file(const char *access_token, const char *folder_id,
                 const char *file_path, const char *file_name) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    fprintf(stderr, "Failed to initialize CURL\n");
    return false;
  }

  // start session
  char init_url[2048];
  snprintf(
      init_url, sizeof(init_url),
      "https://www.googleapis.com/upload/drive/v3/files?uploadType=resumable");

  struct curl_slist *headers = NULL;
  char auth_header[512];
  snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s",
           access_token);
  headers = curl_slist_append(headers, auth_header);

  char metadata[1024];
  snprintf(metadata, sizeof(metadata),
           "{\"name\": \"%s\", \"parents\": [\"%s\"]}", file_name, folder_id);

  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(
      headers, "X-Upload-Content-Type: application/octet-stream");

  char response_buffer[4096] = {0};
  ResponseBuffer response_buf = {
      .data = response_buffer, .size = 0, .capacity = sizeof(response_buffer)};

  // Buffer to get headers
  char header_buffer[8192] = {0};
  ResponseBuffer header_buf = {
      .data = header_buffer, .size = 0, .capacity = sizeof(header_buffer)};

  curl_easy_setopt(curl, CURLOPT_URL, init_url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, metadata);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buf);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_buf);

  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "CURL error initiating upload: %s\n",
            curl_easy_strerror(res));
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return false;
  }

  // check session url
  char *session_uri = extract_location_header(header_buf.data);
  if (!session_uri) {
    fprintf(stderr, "Failed to get session URI from response\n");
    fprintf(stderr, "Headers received:\n%s\n", header_buf.data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return false;
  }

  printf("Session URI obtained: %.80s...\n", session_uri);

  curl_slist_free_all(headers);

  FILE *file = fopen(file_path, "rb");
  if (!file) {
    fprintf(stderr, "Failed to open file: %s\n", file_path);
    free(session_uri);
    curl_easy_cleanup(curl);
    return false;
  }

  fseek(file, 0, SEEK_END);
  curl_off_t file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  printf("Uploading %s (%.2f MB)...\n", file_name,
         file_size / (1024.0 * 1024.0));

  UploadFileContext file_ctx = {.file = file, .size = file_size};

  headers = NULL;
  headers =
      curl_slist_append(headers, "Content-Type: application/octet-stream");

  // buffers reset
  memset(response_buffer, 0, sizeof(response_buffer));
  response_buf.size = 0;

  curl_easy_setopt(curl, CURLOPT_URL, session_uri);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
  curl_easy_setopt(curl, CURLOPT_READDATA, &file_ctx);
  curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, file_size);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buf);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, NULL);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, NULL);

  res = curl_easy_perform(curl);

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  fclose(file);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  free(session_uri);

  if (res != CURLE_OK) {
    fprintf(stderr, "CURL error during file upload: %s\n",
            curl_easy_strerror(res));
    return false;
  }

  if (http_code != 200 && http_code != 201) {
    fprintf(stderr, "HTTP error during upload: %ld\n", http_code);
    fprintf(stderr, "Response: %s\n", response_buf.data);
    return false;
  }

  printf("File uploaded successfully!\n");
  return true;
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

  printf("successfully refreshed access token\n");

  struct json_object *json = json_tokener_parse(response);

  if (json == NULL) {
    fprintf(stderr, "Failed to parse token response\n");
    return false;
  }

  struct json_object *access_token = NULL;
  if (!json_object_object_get_ex(json, "access_token", &access_token) ||
      !json_object_is_type(access_token, json_type_string)) {
    fprintf(stderr, "access_token not found in response\n");
    json_object_put(json);
    return false;
  }

  const char *access_token_value = json_object_get_string(access_token);
  strncpy(provider->access_token, access_token_value,
          sizeof(provider->access_token) - 1);
  provider->access_token[sizeof(provider->access_token) - 1] = '\0';

  json_object_put(json);

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
