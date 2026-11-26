#ifndef FS_H
#define FS_H
#include "common.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

typedef enum { INODE_FREE=0, INODE_FILE=1, INODE_DIR=2 } inode_type_t;
typedef struct {
    uint8_t type;
    uint8_t reserved0;
    uint16_t permissions;
    uint16_t owner;
    uint16_t reserved1;
    uint32_t size;
    uint32_t block_count;
    uint32_t direct[10];
    uint32_t indirect;
    uint8_t reserved[4];
} inode_t;
typedef struct {
    char name[28];
    uint32_t inode_no;
} dir_entry_t;
typedef struct {
    uint32_t magic, block_count, inode_count, free_blocks, free_inodes;
    uint8_t reserved[BLOCK_SIZE-20];
} superblock_t;
typedef struct { uint32_t inode_no; uint32_t offset; int mode; int used; } open_file_t;

/* On-disk layout: 0 superblock; 1 bitmap; 2-17 inode table; 18-2047 data. */
int fs_format(void);
int fs_mount(void);
int fs_unmount(void);
int fs_create(const char *path, uint16_t user, uint32_t cwd);
int fs_delete(const char *path, uint32_t cwd);
int fs_open(const char *path, int mode, uint32_t cwd);
int fs_close(int fd);
ssize_t fs_read(int fd, void *buf, size_t n, uint32_t off);
ssize_t fs_write(int fd, const void *buf, size_t n, uint32_t off);
int fs_mkdir(const char *path, uint32_t cwd);
int fs_cd(const char *path, uint32_t cwd); /* returns new cwd inode, or negative errno */
int fs_ls(const char *path, uint32_t cwd, char *out, size_t out_size);
int fs_report(char *out, size_t out_size);
int fs_check(void);
uint32_t fs_root_inode(void);
#endif
