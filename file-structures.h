#ifndef __FILE_STRUCTURES__H
#define __FILE_STRUCTURES__H

#include "disk.h"

#define TABLE_SIZE 11

typedef struct superblock *superblock_t;

typedef struct inode *inode_t;

typedef struct dir_data_block *dir_data_block_t;

typedef struct free_data_block *free_data_block_t;

#endif __FILE_STRUCTURES_H
