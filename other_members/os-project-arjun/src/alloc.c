#include "fs_internal.h"
#include <errno.h>
int alloc_block(void){ unsigned char bm[BLOCK_SIZE]; if(disk_read(1,bm)<0)return -EIO; for(unsigned b=DATA_START;b<NUM_BLOCKS;b++) if(!(bm[b/8]&(1u<<(b%8)))) { bm[b/8]|=(unsigned char)(1u<<(b%8)); if(disk_write(1,bm)<0)return -EIO; return (int)b; } return -ENOSPC; }
int free_block(uint32_t b){ if(b<DATA_START||b>=NUM_BLOCKS)return -EINVAL; unsigned char bm[BLOCK_SIZE]; disk_read(1,bm); bm[b/8]&=(unsigned char)~(1u<<(b%8)); return disk_write(1,bm); }
int alloc_inode(uint8_t type,uint16_t owner){ inode_t t; for(uint32_t i=1;i<NUM_INODES;i++){ inode_read(i,&t); if(t.type==INODE_FREE){ t=(inode_t){0}; t.type=type;t.owner=owner;t.permissions=0644; inode_write(i,&t); return (int)i; }} return -ENOSPC; }
int free_inode(uint32_t i){ if(i==0||i>=NUM_INODES)return -EINVAL; inode_t z={0}; return inode_write(i,&z); }
