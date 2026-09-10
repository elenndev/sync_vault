#include "core/cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define CACHE_PATH_SIZE 512

static char cache_path[CACHE_PATH_SIZE];

bool cache_init(void) {
  const char *xdg_cache = getenv("XDG_CACHE_HOME");

  if (xdg_cache && xdg_cache[0] != '\0') {
    snprintf(cache_path, sizeof(cache_path), "%s/%s", xdg_cache,
             CACHE_DIR_NAME);
  } else {
    const char *home = getenv("HOME");

    if (!home || home[0] == '\0') {
      fprintf(stderr, "Error: HOME environment variable is not set\n");
      return false;
    }

    snprintf(cache_path, sizeof(cache_path), "%s/.cache/%s", home,
             CACHE_DIR_NAME);
  }

  if (mkdir(cache_path, 0755) != 0) {
    struct stat st;

    if (stat(cache_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
      fprintf(stderr, "Error: Failed to create cache directory: %s\n",
              cache_path);
      return false;
    }
  }

  return true;
}

const char *cache_get_path(void) {
  if (cache_path[0] == '\0') {
    return NULL;
  }

  return cache_path;
}
