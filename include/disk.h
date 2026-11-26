#ifndef DISK_H
#define DISK_H
#include "common.h"
#include <stddef.h>
#include <stdint.h>
int disk_init(void);
int disk_load(const char *path);
int disk_save(const char *path);
int disk_raw_read(uint32_t block, void *buffer);
int disk_raw_write(uint32_t block, const void *buffer);
void disk_zero(void);
uint64_t disk_read_count(void);
uint64_t disk_write_count(void);
void disk_reset_counters(void);
/* Compatibility layer: Day 7 cache/scheduler wrappers can replace these. */
int disk_read(uint32_t block, void *buffer);
int disk_write(uint32_t block, const void *buffer);
#endif
