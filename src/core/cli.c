#include "core/cli.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

bool get_password(char *password, size_t size) {
  struct termios old_term, new_term;

  if (tcgetattr(STDIN_FILENO, &old_term) != 0) {
    return false;
  }

  new_term = old_term;
  new_term.c_lflag &= ~(ECHO);

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_term) != 0) {
    return false;
  }

  printf("Password: ");
  fflush(stdout);

  if (fgets(password, size, stdin) == NULL) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term);
    return false;
  }

  if (strchr(password, '\n') == NULL) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { /* descarta */
    }
  }

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term);

  printf("\n");

  password[strcspn(password, "\n")] = '\0';

  return true;
}
