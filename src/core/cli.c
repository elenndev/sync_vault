#include "core/cli.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

bool is_on_get_input = false;
void enter_get_input(void) { is_on_get_input = true; }
void end_get_input(void) { is_on_get_input = false; }

bool get_password(char *password, size_t size) {
  struct termios old_term, new_term;

  enter_get_input();

  if (tcgetattr(STDIN_FILENO, &old_term) != 0) {
    end_get_input();
    return false;
  }

  new_term = old_term;
  new_term.c_lflag &= ~(ECHO);

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_term) != 0) {
    end_get_input();
    return false;
  }

  printf("Password: ");
  fflush(stdout);

  if (fgets(password, size, stdin) == NULL) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term);
    end_get_input();
    return false;
  }

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term);

  printf("\n");

  password[strcspn(password, "\n")] = '\0';

  end_get_input();

  return true;
}
