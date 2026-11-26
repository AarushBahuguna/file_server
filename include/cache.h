#ifndef CACHE_H
#define CACHE_H
#include "common.h"
#define CACHE_ENTRIES 64
typedef struct { unsigned long hits; unsigned long misses; unsigned long evictions; unsigned long writebacks; } cache_stats_t;
int cache_init(void);
int cache_get(int block_no, void *buf);
int cache_put(int block_no, const void *buf);
int cache_flush(void);
void cache_stats(void);
void cache_get_stats(cache_stats_t *stats);
void cache_destroy(void);
#endif
