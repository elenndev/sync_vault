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

bool get_user_confirm(const char *prompt) {
  struct termios old_term, new_term;

  if (tcgetattr(STDIN_FILENO, &old_term) != 0) {
    return false;
  }

  new_term = old_term;
  new_term.c_lflag &= ~(ICANON | ECHO);
  new_term.c_cc[VMIN] = 1;
  new_term.c_cc[VTIME] = 0;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_term) != 0) {
    return false;
  }

  printf("%s [y/n] ", prompt);
  fflush(stdout);

  char c = 0;
  bool got_answer = false;

  while (1) {
    char ch;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n != 1)
      break;

    if (ch == '\n' || ch == '\r') {
      break; // Enter
    }

    if (ch == 0x7F || ch == 0x08) {
      if (got_answer) {
        printf("\b \b");
        fflush(stdout);
        c = 0;
        got_answer = false;
      }
      continue;
    }

    if (!got_answer) {
      c = ch;
      got_answer = true;
      printf("%c", c);
      fflush(stdout);
    }
  }

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term);
  printf("\n");

  if (!got_answer)
    return false;
  return c == 'y' || c == 'Y';
}
