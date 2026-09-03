#ifndef STATE_H
#define STATE_H

#include <stdbool.h>
typedef struct {
  char last_sync[64];

  char last_backup_name[256];

  char last_backup_hash[65];

} State;

bool state_load();

bool state_save(const State *state);

#endif
