#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdbool.h>

typedef void (*oauth_callback_t)(const char *code, const char *state);

bool http_server_start(int port, oauth_callback_t callback);

void http_server_stop(void);

bool http_server_is_running(void);

#endif
