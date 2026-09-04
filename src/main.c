#include "core/state.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
  // State state = {0};

  if (argc < 2) {
    printf("Usage: %s <command>\n", argv[0]);
    printf("Available commands: start\n");
    return 1;
  }

  if (strcmp(argv[1], "start") == 0) {
    if (state_load()) {
      return 0;
    };
    return 1;

  } else {
    printf("Unknown command: %s\n", argv[1]);
    printf("Available commands: start\n");
    return 1;
  };
}
