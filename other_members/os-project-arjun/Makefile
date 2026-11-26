CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -g -D_POSIX_C_SOURCE=200809L -Iinclude -Isrc
LDFLAGS ?= -pthread
SRC = src/disk.c src/alloc.c src/dir.c src/fs.c
OBJ = $(SRC:.c=.o)
.PHONY: all test clean asan valgrind
all: test_fs
test_fs: $(OBJ) tests/test_fs.o
	$(CC) $(CFLAGS) -o $@ $(OBJ) tests/test_fs.o $(LDFLAGS)
test: test_fs
	./test_fs
asan: CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
asan: LDFLAGS += -fsanitize=address,undefined
asan: clean test_fs
	./test_fs
valgrind: test_fs
	valgrind --leak-check=full --error-exitcode=1 ./test_fs
clean:
	rm -f $(OBJ) tests/*.o test_fs disk.img
