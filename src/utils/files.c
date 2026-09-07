#include "utils/files.h"
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>

bool directory_exists(const char *path) {
  DIR *dir = opendir(path);
  if (dir) {
    closedir(dir);
    return true;
  }

  return false;
}

bool file_exists(const char *path) {
  FILE *file = fopen(path, "r");
  if (file) {
    fclose(file);
    return true;
  }
  return false;
}
