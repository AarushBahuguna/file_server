#include "disk.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
static unsigned char disk[NUM_BLOCKS][BLOCK_SIZE];
static uint64_t reads, writes;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
int disk_init(void){ return 0; }
void disk_zero(void){ pthread_mutex_lock(&lock); memset(disk,0,sizeof disk); reads=writes=0; pthread_mutex_unlock(&lock); }
int disk_load(const char *path){ FILE *f=fopen(path,"rb"); if(!f) return -errno; pthread_mutex_lock(&lock); size_t n=fread(disk,1,sizeof disk,f); pthread_mutex_unlock(&lock); int extra=fgetc(f); fclose(f); if(n!=sizeof disk || extra!=EOF) return -EINVAL; return 0; }
int disk_save(const char *path){ FILE *f=fopen(path,"wb"); if(!f) return -errno; pthread_mutex_lock(&lock); size_t n=fwrite(disk,1,sizeof disk,f); fflush(f); pthread_mutex_unlock(&lock); int close_ok=(fclose(f)==0); int ok=(n==sizeof disk && close_ok); return ok?0:-EIO; }
int disk_raw_read(uint32_t b,void *buf){ if(b>=NUM_BLOCKS||!buf)return -EINVAL; pthread_mutex_lock(&lock); memcpy(buf,disk[b],BLOCK_SIZE); reads++; pthread_mutex_unlock(&lock); return 0; }
int disk_raw_write(uint32_t b,const void *buf){ if(b>=NUM_BLOCKS||!buf)return -EINVAL; pthread_mutex_lock(&lock); memcpy(disk[b],buf,BLOCK_SIZE); writes++; pthread_mutex_unlock(&lock); return 0; }
int disk_read(uint32_t b,void *buf){ return disk_raw_read(b,buf); }
int disk_write(uint32_t b,const void *buf){ return disk_raw_write(b,buf); }
uint64_t disk_read_count(void){ return reads; } uint64_t disk_write_count(void){ return writes; }
void disk_reset_counters(void){ reads=writes=0; }
