#include "http/http_server.h"
#include "utils/logger.h"
#include <microhttpd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RESPONSE_SIZE 4096

static struct MHD_Daemon *daemon = NULL;
static oauth_callback_t user_callback = NULL;

typedef struct {
  char provider[64];
  char code[256];
  char state[128];
} OAuthRequestData;

static enum MHD_Result
handle_request(void *cls, struct MHD_Connection *connection, const char *url,
               const char *method, const char *version, const char *upload_data,
               size_t *upload_data_size, void **ptr) {

  // Só aceita GET
  if (strcmp(method, "GET") != 0) {
    struct MHD_Response *response =
        MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
    enum MHD_Result ret =
        MHD_queue_response(connection, MHD_HTTP_METHOD_NOT_ALLOWED, response);
    MHD_destroy_response(response);
    return ret;
  }

  // Verifica se é a rota de callback
  if (strncmp(url, "/oauth2callback/", 16) == 0) {
    const char *provider =
        url + 16; // Pega o nome do provider após /oauth2callback/

    // Extrai parâmetros da query string
    const char *code =
        MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "code");
    const char *state =
        MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "state");

    if (code && user_callback) {
      log_info("Callback recebido para provider: %s", provider);
      user_callback(code, state ? state : "");

      // Responde com página de sucesso
      const char *page = "<html><body><h1>Autorização concluída!</h1>"
                         "<p>Você pode fechar esta janela.</p></body></html>";
      struct MHD_Response *response = MHD_create_response_from_buffer(
          strlen(page), (void *)page, MHD_RESPMEM_PERSISTENT);
      enum MHD_Result ret =
          MHD_queue_response(connection, MHD_HTTP_OK, response);
      MHD_destroy_response(response);

      // Para o servidor automaticamente após receber o callback
      http_server_stop();
      return ret;
    } else {
      // Responde com erro se faltar parâmetro
      const char *page =
          "<html><body><h1>Erro na autorização</h1>"
          "<p>Faltam parâmetros na requisição.</p></body></html>";
      struct MHD_Response *response = MHD_create_response_from_buffer(
          strlen(page), (void *)page, MHD_RESPMEM_PERSISTENT);
      enum MHD_Result ret =
          MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
      MHD_destroy_response(response);
      return ret;
    }
  }

  // Rota raiz
  if (strcmp(url, "/") == 0) {
    const char *page = "<html><body><h1>Servidor de Callback OAuth</h1>"
                       "<p>Aguardando autorização...</p></body></html>";
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(page), (void *)page, MHD_RESPMEM_PERSISTENT);
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
  }

  // 404 para outras rotas
  struct MHD_Response *response =
      MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
  enum MHD_Result ret =
      MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
  MHD_destroy_response(response);
  return ret;
}

bool http_server_start(int port, oauth_callback_t callback) {
  if (daemon) {
    log_warning("server HTTP already running");
  }

  user_callback = callback;

  daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG, port,
                            NULL, NULL, &handle_request, NULL, MHD_OPTION_END);

  if (!daemon) {
    log_error("Failed when start HTTP on port %d", port);
    return false;
  }

  log_info("Server started on http://localhost:%d", port);
  log_info("Callback url: http://localhost:%d/oauth2callback/{provider}", port);

  return true;
}

void http_server_stop(void) {
  if (daemon) {
    MHD_stop_daemon(daemon);
    daemon = NULL;
    log_info("Server http ended");
  }
}

bool http_server_is_running(void) { return daemon != NULL; }
