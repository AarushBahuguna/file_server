# Shared Makefile. Each member adds their own targets in their section.
CC      = gcc
CFLAGS  = -Wall -Wextra -g -pthread -Iinclude
LDFLAGS = -pthread
COMMON  = src/common/ipc.c src/common/protocol.c src/common/log.c src/common/session.c src/common/stats.c

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

# ---- Shubham: Day 4 (log + session) ----
bin/test_session: tests/test_session.c src/common/session.c
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

bin/test_log: tests/test_log.c src/common/log.c src/common/protocol.c
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

unit-test: bin/test_protocol bin/test_parse bin/test_session bin/test_log
	./bin/test_protocol
	./bin/test_parse
	./bin/test_session
	./bin/test_log

check: unit-test echo-test cli-test
	@echo ALL CHECKS PASSED


# ---- Shubham: Day 5 (stats formatting + ops scripts) ----
bin/test_stats: tests/test_stats.c src/common/stats.c
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

bin/fake_fs_server: tests/fake_fs_server.c $(COMMON)
	@mkdir -p bin
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

stats-test: bin/test_stats
	./bin/test_stats

ops-test: bin/client bin/fake_fs_server
	./scripts/run_ops.sh

check: stats-test ops-test

# ---- Shubham: Day 6 IPC reliability ----
.PHONY: ipc-test
ipc-test:
	./scripts/test_ipc_day6.sh

check: ipc-test
