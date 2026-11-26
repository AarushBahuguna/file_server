#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include "cache.h"
#include "sched.h"

#define HASH_SIZE 127
#define CACHE_ENTRIES 64

typedef struct CacheEntry {
    int block_no, valid, dirty;
    unsigned char data[BLOCK_SIZE];
    struct CacheEntry *prev, *next, *hash_next;
} CacheEntry;

static CacheEntry entries[CACHE_ENTRIES];
static CacheEntry *hash_table[HASH_SIZE];
static CacheEntry *lru_head, *lru_tail;
static pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;
static cache_stats_t stats;

static unsigned hash_block(int block) { return (unsigned)block % HASH_SIZE; }
static CacheEntry *find_entry(int block) { CacheEntry *cur = hash_table[hash_block(block)]; while (cur) { if (cur->valid && cur->block_no == block) return cur; cur = cur->hash_next; } return NULL; }
static void remove_lru(CacheEntry *entry) { if (entry->prev) entry->prev->next = entry->next; else lru_head = entry->next; if (entry->next) entry->next->prev = entry->prev; else lru_tail = entry->prev; entry->prev = entry->next = NULL; }
static void insert_front(CacheEntry *entry) { entry->prev = NULL; entry->next = lru_head; if (lru_head) lru_head->prev = entry; lru_head = entry; if (!lru_tail) lru_tail = entry; }
static void move_front(CacheEntry *entry) { if (entry == lru_head) return; remove_lru(entry); insert_front(entry); }
static void hash_insert(CacheEntry *entry) { unsigned h = hash_block(entry->block_no); entry->hash_next = hash_table[h]; hash_table[h] = entry; }
static void hash_remove(CacheEntry *entry) { unsigned h = hash_block(entry->block_no); CacheEntry **cur = &hash_table[h]; while (*cur) { if (*cur == entry) { *cur = entry->hash_next; entry->hash_next = NULL; return; } cur = &(*cur)->hash_next; } }

static int disk_read_block(int block, void *buffer) {
    struct disk_req req = {0}; req.block = block; req.is_write = 0; req.buf = buffer; sem_init(&req.done, 0, 0); int rc = sched_submit(&req); sem_destroy(&req.done); return rc;
}
static int disk_write_block(int block, const void *buffer) {
    struct disk_req req = {0}; req.block = block; req.is_write = 1; req.buf = (void *)buffer; sem_init(&req.done, 0, 0); int rc = sched_submit(&req); sem_destroy(&req.done); return rc;
}

int cache_init(void) { pthread_mutex_lock(&cache_mutex); memset(entries, 0, sizeof entries); memset(hash_table, 0, sizeof hash_table); memset(&stats, 0, sizeof stats); lru_head = lru_tail = NULL; for (int i = 0; i < CACHE_ENTRIES; i++) insert_front(&entries[i]); pthread_mutex_unlock(&cache_mutex); return 0; }
int cache_get(int block_no, void *buffer) { if (block_no < 0 || (unsigned)block_no >= NUM_BLOCKS || !buffer) return -EINVAL; pthread_mutex_lock(&cache_mutex); CacheEntry *entry = find_entry(block_no); if (entry) { stats.hits++; move_front(entry); memcpy(buffer, entry->data, BLOCK_SIZE); pthread_mutex_unlock(&cache_mutex); return 0; } stats.misses++; entry = lru_tail; if (!entry) { pthread_mutex_unlock(&cache_mutex); return -ENOMEM; } if (entry->valid) { if (entry->dirty) { if (disk_write_block(entry->block_no, entry->data) < 0) { pthread_mutex_unlock(&cache_mutex); return -EIO; } stats.writebacks++; } hash_remove(entry); stats.evictions++; } pthread_mutex_unlock(&cache_mutex); if (disk_read_block(block_no, entry->data) < 0) return -EIO; pthread_mutex_lock(&cache_mutex); entry->block_no = block_no; entry->valid = 1; entry->dirty = 0; hash_insert(entry); move_front(entry); memcpy(buffer, entry->data, BLOCK_SIZE); pthread_mutex_unlock(&cache_mutex); return 0; }
int cache_put(int block_no, const void *buffer) { if (block_no < 0 || (unsigned)block_no >= NUM_BLOCKS || !buffer) return -EINVAL; pthread_mutex_lock(&cache_mutex); CacheEntry *entry = find_entry(block_no); if (entry) { memcpy(entry->data, buffer, BLOCK_SIZE); entry->dirty = 1; move_front(entry); pthread_mutex_unlock(&cache_mutex); return 0; } entry = lru_tail; if (!entry) { pthread_mutex_unlock(&cache_mutex); return -ENOMEM; } if (entry->valid) { if (entry->dirty) { if (disk_write_block(entry->block_no, entry->data) < 0) { pthread_mutex_unlock(&cache_mutex); return -EIO; } stats.writebacks++; } hash_remove(entry); stats.evictions++; } entry->block_no = block_no; entry->valid = 1; entry->dirty = 1; memcpy(entry->data, buffer, BLOCK_SIZE); hash_insert(entry); move_front(entry); pthread_mutex_unlock(&cache_mutex); return 0; }
int cache_flush(void) { pthread_mutex_lock(&cache_mutex); for (int i = 0; i < CACHE_ENTRIES; i++) { CacheEntry *entry = &entries[i]; if (entry->valid && entry->dirty) { if (disk_write_block(entry->block_no, entry->data) < 0) { pthread_mutex_unlock(&cache_mutex); return -EIO; } entry->dirty = 0; stats.writebacks++; } } pthread_mutex_unlock(&cache_mutex); return 0; }
void cache_get_stats(cache_stats_t *result) { if (!result) return; pthread_mutex_lock(&cache_mutex); *result = stats; pthread_mutex_unlock(&cache_mutex); }
void cache_stats(void) { cache_stats_t s; cache_get_stats(&s); unsigned total = (unsigned)(s.hits + s.misses); printf("Hits: %lu\nMisses: %lu\nEvictions: %lu\nWritebacks: %lu\nHit Ratio: %.2f%%\n", s.hits, s.misses, s.evictions, s.writebacks, total ? 100.0 * s.hits / total : 0.0); }
void cache_destroy(void) { cache_flush(); pthread_mutex_destroy(&cache_mutex); }
