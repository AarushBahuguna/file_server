# Shared Makefile. Each member adds their own targets in their section.
CC      = gcc
CFLAGS  = -Wall -Wextra -g -pthread -Iinclude
LDFLAGS = -pthread
COMMON  = src/common/ipc.c src/common/protocol.c

.PHONY: all test clean

all: bin/client bin/test_protocol
# TODO Arjun:    add fs / disk targets here
# TODO Shwetank: add bin/server (lock manager, cache, thread pool) here
# TODO Aarush:   add sched / waitfor / deadlock targets here

# ---- Shubham: client + IPC ----
bin/client: src/client/main.c src/client/parse.c $(COMMON)
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

bin/test_protocol: tests/test_protocol.c $(COMMON)
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test: bin/test_protocol
	./bin/test_protocol

clean:
	rm -rf bin

# ---- Shubham: Day 2 echo test ----
bin/echo_server: tests/echo_server.c $(COMMON)
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

bin/echo_client: tests/echo_client.c $(COMMON)
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

echo-test: bin/echo_server bin/echo_client
	./scripts/test_echo.sh

# ---- Shubham: Day 3 CLI ----
bin/test_parse: tests/test_parse.c src/client/parse.c
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

bin/dummy_server: tests/dummy_server.c $(COMMON)
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

cli-test: bin/test_parse bin/dummy_server bin/client bin/echo_client
	./bin/test_parse
	./scripts/test_cli.sh
