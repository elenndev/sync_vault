#include "providers/google_drive.h"
#include "http/http_server.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <stdio.h>
#include <string.h>

static GoogleDriveProvider *g_provider = NULL;

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

bool google_drive_refresh_access_token(GoogleDriveProvider *provider) {
  CURL *curl = curl_easy_init();

  if (!curl)
    return false;

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
  if (http_code != 200) {
    fprintf(stderr, "Google drive http error: %ld\n", http_code);
    fprintf(stderr, "Response: %s\n", response);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return false;
  }

  if (result != CURLE_OK) {
    fprintf(stderr, "Curl Error: %s\n", curl_easy_strerror(result));

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return false;
  }

  printf("response:\n%s\n", response);

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  return true;
}

bool google_drive_authenticate(GoogleDriveProvider *provider) {
  // first i have to start a http server
  //
  // then continue...

  CURL *curl = curl_easy_init();

  if (!curl)
    return false;

  char auth_url[4096];
  const char *redirect_uri = "http://127.0.0.1:8080/callback";

  snprintf(auth_url, sizeof(auth_url),
           "https://accounts.google.com/o/oauth2/v2/auth"
           "?client_id=%s"
           "&redirect_uri=%s"
           "&response_type=code"
           "&scope=https%%3A%%2F%%2Fwww.googleapis.com%%2Fauth%%2Fdrive.file"
           "&access_type=offline"
           "&prompt=consent",
           provider->client_id, redirect_uri);
}

static void handle_oauth_callback(const char *provider, const char *code,
                                  const char *state) {
  if (!g_provider) {
    fprintf(stderr, "Provider not set for callback\n");
    return;
  }

  if (strcmp(provider, "google_drive") != 0) {
    fprintf(stderr, "Unexpected provider: %s\n", provider);
    return;
  }

  if (!code) {
    fprintf(stderr, "Authorization code not received\n");
    return;
  }

  // 🔒 VALIDA O STATE
  if (!state || !g_provider->expected_state[0]) {
    fprintf(stderr, "⚠️ State validation failed: missing state\n");
    return;
  }

  if (strcmp(state, g_provider->expected_state) != 0) {
    fprintf(stderr, "❌ State validation failed!\n");
    fprintf(stderr, "   Expected: %s\n", g_provider->expected_state);
    fprintf(stderr, "   Received: %s\n", state);
    fprintf(stderr, "   Possible CSRF attack detected!\n");
    return;
  }

  printf("\n✅ State validated successfully!\n");
  printf("✅ Authorization code received: %s\n", code);

  // Troca o código por tokens (passando o state também)
  if (!google_drive_exchange_code(g_provider, code, state)) {
    fprintf(stderr, "Failed to exchange code for tokens\n");
    return;
  }

  printf("✅ Tokens obtained successfully!\n");
  printf("   Access Token: %s\n", g_provider->access_token);

  // Para o servidor após obter o token
  http_server_stop();
}

bool google_drive_exchange_code(GoogleDriveProvider *provider, const char *code,
                                const char *state) {
  if (!provider || !code) {
    fprintf(stderr, "Invalid parameters for exchange\n");
    return false;
  }

  // Valida o state novamente (por segurança)
  if (!state || !provider->expected_state[0]) {
    fprintf(stderr, "State validation failed in exchange\n");
    return false;
  }

  if (strcmp(state, provider->expected_state) != 0) {
    fprintf(stderr, "states don't match.\n");
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

  strncpy(provider->access_token, response, sizeof(provider->access_token) - 1);
  provider->access_token[sizeof(provider->access_token) - 1] = '\0';

  return true;
}
