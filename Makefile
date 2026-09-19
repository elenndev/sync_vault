CC = gcc

CFLAGS = -Wall -Wextra -std=c17 -Iinclude
LIBS = -lcurl -lmicrohttpd -ljson-c 

SRC := $(shell find src -name "*.c")

BIN = build/bin/sync-vault
PREFIX = /usr/local
INSTALL_DIR = $(PREFIX)/bin

all:
	mkdir -p build/bin
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LIBS)

# Executa sem argumentos
run: all
	./$(BIN)

run-args: all
	./$(BIN) $(ARGS)

r: all
	./$(BIN) $(ARGS)

install: all
	install -d $(INSTALL_DIR)
	install -m 0755 $(BIN) $(INSTALL_DIR)/sync-vault
	@echo "Installed: $(INSTALL_DIR)/sync-vault"

uninstall:
	rm -f $(INSTALL_DIR)/sync-vault
	@echo "Removed: $(INSTALL_DIR)/sync-vault"

clean:
	rm -rf build
