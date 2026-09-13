#ifndef GOOGLEDRIVE_H
#define GOOGLEDRIVE_H

#include "files/files.h"
#include <curl/system.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

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
  char client_id[256];
  char client_secret[256];
  char folder_id[128];
} GoogleDriveConfig;

typedef struct {
  char *data;
  size_t size;
  size_t capacity;
} ResponseBuffer;

typedef struct {
  FILE *file;
  curl_off_t size;
} UploadFileContext;

bool google_drive_init(GoogleDriveProvider *provider);
bool download_file(char *access_token, char *file_id, char *file_name,
                   const char *cache_path);
bool list_folder_files(char *access_token, char *folder_id,
                       LatestArchive *out_latest);
bool upload_file(const char *acess_token, const char *folder_id,
                 const char *file_path, const char *file_name);
bool google_drive_authenticate(GoogleDriveProvider *provider);
bool google_drive_refresh_access_token(GoogleDriveProvider *provider);
bool google_drive_exchange_code(GoogleDriveProvider *provider, const char *code,
                                const char *state);

#endif
