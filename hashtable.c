#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashtable.h"

typedef struct hashtable_node {
    // Need to save because indexed by hash
    void *key;
    void *value;
} *hashtable_node_t;

struct hashtable {
    //maximum size of hash table
    unsigned int max_size;
    //filled hash table size
    unsigned int size;
    hashFunc hash;
    equalsFunc equals;
    //hash node - <key,value> pair
    hashtable_node_t *table;
};

hashtable_t
hashtable_init(unsigned int size, hashFunc func, equalsFunc equals) {
    hashtable_t hashtable;
    
    //validate parameters
    if (size < 1 || func == NULL || equals == NULL) {
        return NULL;
    }
    
    hashtable = (hashtable_t) malloc(sizeof(struct hashtable));
    if (hashtable == NULL) {
        fprintf(stderr, "Out of memory\n");
        return NULL;
    }
    hashtable->hash = func;
    hashtable->equals = equals;
    hashtable->max_size = size;
    hashtable->size = 0;
    hashtable->table = (hashtable_node_t *) malloc(size * sizeof(hashtable_node_t));
    if (hashtable->table == NULL) {
        fprintf(stderr, "Could not allocate hash table memory\n");
        free(hashtable);
    }
    
    memset(hashtable->table, 0, size * sizeof(hashtable_node_t));
    return hashtable;
}

int
hashtable_get(hashtable_t hashtable, void *key, void **value) {
    unsigned int hash_table_index = 0;
    int hash_key_index, hash;
    if(hashtable == NULL || key == NULL) {
        return -1;
    }
    hash = hashtable->hash(key);
    
    hash_key_index = (hash) % hashtable -> max_size;
    hash_key_index = ( hash_key_index < 0 ) ? hash_key_index + hashtable->max_size : hash_key_index;
    
    // search table
    while ((hash_table_index < hashtable->max_size) && ((hashtable->table[hash_key_index] == NULL) ||
            ((hashtable->table[hash_key_index] != NULL) && !(hashtable->equals(key ,hashtable->table[hash_key_index]->key))))) {
        hash_table_index++;
        hash_key_index = (hash_key_index == hashtable->max_size - 1) ? 0 : hash_key_index + 1;
    }
    
    //reached end of hash table
    if (hash_table_index == hashtable->max_size) {
        *value = NULL;
        return -1;
    }
    
    *value = hashtable->table[hash_key_index]->value;
    return 0;
}

void*
hashtable_remove(hashtable_t hashtable, void *key) {
    unsigned int hash_table_index = 0;
    int hash_key_index;
    void *ret;
    
    int hash = hashtable->hash(key);
    
    hash_key_index = (hash) % hashtable->max_size;
    // validate hash_key_index
    hash_key_index = ( hash_key_index < 0 ) ? hash_key_index + hashtable->max_size : hash_key_index;
    
    // search table
    while ((hash_table_index < hashtable->max_size) && ((hashtable->table[hash_key_index] == NULL) || ((hashtable->table[hash_key_index] != NULL) && !(hashtable->equals(key ,hashtable->table[hash_key_index]->key))))) {
        hash_table_index++;
        hash_key_index = (hash_key_index == hashtable->max_size - 1) ? 0 : hash_key_index + 1;
    }
    
    // not in table
    if (hash_table_index == hashtable->max_size) {
        return NULL;
    }
    
    ret = hashtable->table[hash_key_index]->key;
    free(hashtable->table[hash_key_index]);
    hashtable->table[hash_key_index] = NULL;
    (hashtable->size)--;
    
    return ret;
}

int
hashtable_put(hashtable_t hashtable, void *key, void *value) {
    unsigned int hash_table_index = 0;
    int hash_key_index, hash;
    hashtable_node_t temp;
    
    if(hashtable == NULL || key== NULL || value== NULL) {
        return -1;
    }
    hash = hashtable->hash(key);
    
    hash_key_index = (hash) % hashtable->max_size;
    // validate hash key index
    hash_key_index = ( hash_key_index < 0 ) ? hash_key_index + hashtable->max_size : hash_key_index;
    
    // search table
    while ((hash_table_index < hashtable->max_size) && (hashtable->table[hash_key_index] != NULL)) {
        hash_table_index++;
        hash_key_index = (hash_key_index == hashtable->max_size - 1) ? 0 : hash_key_index + 1;
    }
    
    //hash table full
    if (hash_table_index == hashtable->max_size) {
        return -1;
    }
    
    temp = (hashtable_node_t) malloc(sizeof(struct hashtable_node));
    if (temp == NULL) return -1;
    
    temp->key = key;
    temp->value = value;
    hashtable->table[hash_key_index] = temp;
    (hashtable->size)++;
    
    return 0;
}

int
hashtable_size(hashtable_t hashtable) {
    return hashtable->size;
}

void
hashtable_free(hashtable_t hashtable) {
    unsigned int hash_table_index = 0;
    int hash_key_index = 0;
    hashtable_node_t temp;
    
    while (hash_table_index < hashtable->max_size) {
        temp = hashtable->table[hash_key_index];
        if (temp != NULL) free(temp);
        hash_table_index++;
        hash_key_index++;
    }
    
    free(hashtable->table);
    free(hashtable);
}