CC = gcc

CFLAGS = -Wall -Wextra -std=c17 -Iinclude
LIBS = -lcurl -lmicrohttpd

SRC := $(shell find src -name "*.c")

BIN = build/bin/sync-vault

all:
	mkdir -p build/bin
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LIBS)

run: all
	./$(BIN)

clean:
	rm -rf build
