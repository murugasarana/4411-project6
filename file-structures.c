#include "file-structures.h"

struct superblock {
    union {
        
        struct {
            char magic_number[4];
            char disk_size[4];
            
            char root_node[4];
            
            char first_free_inode[4];
            char first_free_data_block[4];
        } data;
        
        char padding[DISK_BLOCK_SIZE];
    }
};

typedef struct inode {
    
    union {
        
        struct {
            char inode_type;
            char size[4];
            
            char direct_ptrs[TABLE_SIZE] [4];
            char indirect_ptr[4];
        } data;
        
        char padding[DISK_BLOCK_SIZE];
    }
};


struct dir_data_block {
    
    union {
        
        struct {
            char dir_entries[TABLE_SIZE][256];
            char inode_ptrs[TABLE_SIZE][4];
            
        } data;
        
        char padding[DISK_BLOCK_SIZE];
    }
};


struct free_data_block {
    
    union {
        char next_free_block[4];
        
        char padding[DISK_BLOCK_SIZE];
    }
};

