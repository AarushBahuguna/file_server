#ifndef FS_INTERNAL_H
#define FS_INTERNAL_H
#include "fs.h"
#include "disk.h"
int inode_read(uint32_t n,inode_t *out);
int inode_write(uint32_t n,const inode_t *in);
int alloc_block(void);
int free_block(uint32_t b);
int alloc_inode(uint8_t type,uint16_t owner);
int free_inode(uint32_t n);
int map_block(inode_t *ino,uint32_t logical,int create);
int dir_lookup(uint32_t dir,const char *name,uint32_t *ino_out);
int dir_add(uint32_t dir,const char *name,uint32_t ino);
int dir_remove(uint32_t dir,const char *name);
int resolve_path(const char *path,uint32_t cwd,uint32_t *ino_out,uint32_t *parent_out,char *leaf);
#endif
