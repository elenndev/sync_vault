#include "providers/google_drive.h"
#include "core/state.h"
#include "http/http_server.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h> // Para sleep()

static GoogleDriveProvider *g_provider = NULL;

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

// ============================================
// FUNÇÕES DE PERSISTÊNCIA (NOVAS)
// ============================================

static bool load_tokens_from_state(GoogleDriveProvider *provider) {
  State *state = state_get(); // ✅ AGORA EXISTE
  if (!state) {
    return false;
  }

  // Verifica se tem token salvo
  if (state->google_access_token[0] == '\0') {
    return false;
  }

  // Restaura tokens
  strncpy(provider->access_token, state->google_access_token,
          sizeof(provider->access_token) - 1);
  provider->access_token[sizeof(provider->access_token) - 1] = '\0';

  strncpy(provider->refresh_token, state->google_refresh_token,
          sizeof(provider->refresh_token) - 1);
  provider->refresh_token[sizeof(provider->refresh_token) - 1] = '\0';

  printf("Tokens carregados do estado\n");
  return true;
}

static bool save_tokens_to_state(GoogleDriveProvider *provider) {
  State *state = state_get();
  if (!state) {
    return false;
  }

  // Salva tokens no state
  strncpy(state->google_access_token, provider->access_token,
          sizeof(state->google_access_token) - 1);
  state->google_access_token[sizeof(state->google_access_token) - 1] = '\0';

  strncpy(state->google_refresh_token, provider->refresh_token,
          sizeof(state->google_refresh_token) - 1);
  state->google_refresh_token[sizeof(state->google_refresh_token) - 1] = '\0';

  // Persiste no disco
  return state_save();
}

static bool is_token_expired(GoogleDriveProvider *provider) {
  // Se não tem token, está expirado
  if (provider->access_token[0] == '\0') {
    return true;
  }

  // Por enquanto, assume que token é válido por 1 hora
  // Ideal: guardar timestamp de expiração
  return false;
}

static void handle_oauth_callback(const char *code, const char *state) {
  if (!g_provider) {
    fprintf(stderr, "Provider not set for callback\n");
    return;
  }

  // Valida state (CSRF)
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

  // Troca código por tokens
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
    printf("Tokens restaurados do estado\n");
    return true;
  }

  printf("enhum token salvo encontrado\n");
  return true;
}

bool google_drive_authenticate(GoogleDriveProvider *provider) {
  if (!provider) {
    return false;
  }

  g_provider = provider;

  if (load_tokens_from_state(provider)) {
    printf("Usando tokens salvos\n");

    // Verifica se o token ainda é válido
    if (!is_token_expired(provider)) {
      printf("Token ainda válido\n");
      return true;
    }

    // Token expirado, tenta refresh
    printf("Token expirado, tentando refresh...\n");
    if (google_drive_refresh_access_token(provider)) {
      printf("Refresh bem sucedido\n");
      return true;
    }

    printf("Refresh falhou, reautenticando...\n");
  }

  printf("Iniciando fluxo OAuth...\n");

  // Gera state (CSRF)
  srand(time(NULL));
  char state[64];
  snprintf(state, sizeof(state), "%d", rand());
  strncpy(provider->expected_state, state,
          sizeof(provider->expected_state) - 1);
  provider->expected_state[sizeof(provider->expected_state) - 1] = '\0';

  // Inicia servidor HTTP (callback com 2 parâmetros)
  if (!http_server_start(8080, handle_oauth_callback)) {
    fprintf(stderr, "Failed to start HTTP server\n");
    return false;
  }

  // Abre URL no navegador
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

  printf("🌐 Navegador aberto para autenticação\n");
  printf("📋 Aguardando callback em %s\n", redirect_uri);

  // Aguarda callback
  while (http_server_is_running()) {
    sleep(1);
  }

  // Verifica se recebeu token
  if (provider->access_token[0] == '\0') {
    fprintf(stderr, "❌ Autenticação falhou ou foi cancelada\n");
    return false;
  }

  printf("✅ Autenticação concluída com sucesso!\n");
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

  // Valida o state novamente
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

  strncpy(provider->access_token, response, sizeof(provider->access_token) - 1);
  provider->access_token[sizeof(provider->access_token) - 1] = '\0';

  if (!save_tokens_to_state(provider)) {
    fprintf(stderr, "Failed to save tokens to state\n");
    return false;
  }

  printf("✅ Tokens saved to state\n");
  return true;
}
