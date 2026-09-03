#include <string.h>
void string_trim(char *str) {
  char *start = str;

  while (*start == ' ' || *start == '\t')
    start++;

  if (start != str)
    memmove(str, start, strlen(start) + 1);

  int len = strlen(str);

  while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' ||
                     str[len - 1] == '\n' || str[len - 1] == '\r')) {
    str[--len] = '\0';
  }
}

void string_remove_quotes(char *str) {
  int len = strlen(str);

  if (len >= 2 && str[0] == '"' && str[len - 1] == '"') {
    memmove(str, str + 1, len - 2);
    str[len - 2] = '\0';
  }
}

void string_copy(char *dest, size_t dest_size, const char *src) {
  strncpy(dest, src, dest_size - 1);
  dest[dest_size - 1] = '\0';
}
