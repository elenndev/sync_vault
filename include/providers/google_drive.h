#ifndef GOOGLEDRIVE_H
#define GOOGLEDRIVE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  char client_id[256];
  char client_secret[256];
  char refresh_token[512];
  char access_token[2048];
  char folder_id[128];
  char auth_code[256];
  char expected_state[64];
} GoogleDriveProvider;

typedef struct {
  char *data;
  size_t size;
  size_t capacity;
} ResponseBuffer;

bool google_drive_init(GoogleDriveProvider *provider);
bool google_drive_authenticate(GoogleDriveProvider *provider);
bool google_drive_refresh_access_token(GoogleDriveProvider *provider);
bool google_drive_exchange_code(GoogleDriveProvider *provider, const char *code,
                                const char *state);

#endif
