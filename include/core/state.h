#ifndef STATE_H
#define STATE_H

#include <stdbool.h>
typedef struct {
  char last_sync[64];
  char last_backup_name[256];
  char last_backup_hash[65];

  char google_access_token[2048];
  char google_refresh_token[512];
  char google_token_expiry[64];

} State;

bool state_load();

bool state_save();

State *state_get(void);

#endif
