#ifndef STATE_H
#define STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>
typedef struct {
  char last_sync[64];
  char last_backup_name[256];
  char last_backup_hash[65];
  char vault_path[512];

  char google_access_token[2048];
  char google_refresh_token[512];
  char google_token_expiry[64];

} State;

typedef enum { SYNC_NONE, SYNC_DOWNLOAD, SYNC_UPLOAD } SyncAction;

typedef void (*SyncCallback)(void);

bool state_load();

bool state_save();

void compare_syncs(time_t last_local_sync_timestamp, time_t backup_timestamp,
                   SyncAction *action);

void get_state_dir(char *buffer, size_t size);

State *state_get(void);

#endif
