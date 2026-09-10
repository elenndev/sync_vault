#ifndef CACHE_H
#define CACHE_H

#include <stdbool.h>

#define CACHE_DIR_NAME "sync-vault"

bool cache_init(void);
const char *cache_get_path(void);

#endif
