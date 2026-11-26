#include <stdio.h>
#include <string.h>
#include "cache.h"
#include "disk.h"
#include "sched.h"

int main(void) {
    unsigned char input[BLOCK_SIZE], output[BLOCK_SIZE];
    memset(input, 0x5A, sizeof(input));
    disk_init(); disk_zero(); sched_init(); cache_init();

    if (cache_put(20, input) != 0) return 1;
    if (cache_flush() != 0) return 1;
    memset(output, 0, sizeof(output));

    cache_stats_t stats;
    cache_get_stats(&stats);
    printf("cache stats after put/flush: hits=%lu misses=%lu evictions=%lu writebacks=%lu\n",
           stats.hits, stats.misses, stats.evictions, stats.writebacks);
    if (stats.writebacks != 1) return 1;

    if (cache_get(20, output) != 0) return 1;
    if (memcmp(input, output, sizeof(input)) == 0) {
        cache_get_stats(&stats);
        printf("cache stats after get: hits=%lu misses=%lu evictions=%lu writebacks=%lu\n",
               stats.hits, stats.misses, stats.evictions, stats.writebacks);
        cache_destroy(); sched_shutdown();
        puts("cache hit and writeback checks passed");
        return 0;
    }

    cache_destroy(); sched_shutdown();
    return 1;
}
