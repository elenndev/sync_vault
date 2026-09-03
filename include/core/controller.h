#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdbool.h>
typedef enum { ADD_VAULT_DIR, SYNC_VAULT_DIR } Command;

bool run_command(Command command);

#endif
