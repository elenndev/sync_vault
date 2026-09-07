#ifndef STATE_H
#define STATE_H

#include <stdbool.h>
#include <stddef.h>
typedef struct {
  char last_sync[64];
  char last_backup_name[256];
  char last_backup_hash[65];
  char vault_path[512];

  char google_access_token[2048];
  char google_refresh_token[512];
  char google_token_expiry[64];

} State;

bool state_load();

bool state_save();

void get_state_dir(char *buffer, size_t size);

State *state_get(void);

#endif
