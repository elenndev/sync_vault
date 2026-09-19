#ifndef FILESUTILS_H
#define FILESUTILS_H

#include <stdbool.h>
bool directory_exists(const char *path);
bool file_exists(const char *path);
bool is_directory_empty(const char *path);

#endif
