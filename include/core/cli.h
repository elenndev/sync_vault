#ifndef CLI_H
#define CLI_H
#include <stdbool.h>
#include <stddef.h>

// typedef enum { YES, NO } UserConfirm;

bool get_password(char *password, size_t size);

bool get_user_confirm(const char *prompt);

#endif
