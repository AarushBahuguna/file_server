#ifndef COMMON_H
#define COMMON_H
#include <stdint.h>
#define BLOCK_SIZE 512u
#define NUM_BLOCKS 2048u
#define NUM_INODES 128u
#define DATA_START 18u
#define MAX_NAME 28u
#define MAX_PATH 256u
#define MAX_OPEN_FILES 256u
#define FS_MAGIC 0x4D465331u
#define FS_MAX_FILE_SIZE ((10u + 128u) * BLOCK_SIZE)
#endif
