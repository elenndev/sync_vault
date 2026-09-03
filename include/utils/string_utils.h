#ifndef STRINGUTILS_H
#define STRINGUTILS_H

#include <stddef.h>
void string_trim(char *str);
void string_remove_quotes(char *str);
void string_copy(char *dest, size_t dest_size, const char *src);

#endif
