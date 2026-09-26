#ifndef STATE_H
#define STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>
typedef struct {
  time_t last_sync;
  char vault_path[512];
  time_t last_backup_timestamp;

  char google_access_token[2048];
  char google_refresh_token[512];
  char google_token_expiry[64];

} State;

typedef enum { SYNC_NONE, SYNC_DOWNLOAD, SYNC_UPLOAD } SyncAction;
typedef enum {
  STATE_LOAD_OK = 0,
  STATE_LOAD_ERR_DIR,
  STATE_LOAD_NOT_FOUND_AUTH_NEED,
} StateLoadResult;

typedef void (*SyncCallback)(void);

StateLoadResult state_load();

bool state_save();

void compare_syncs(time_t last_local_sync_timestamp, time_t backup_timestamp,
                   SyncAction *action);

void get_state_dir(char *buffer, size_t size);

State *state_get(void);

#endif
