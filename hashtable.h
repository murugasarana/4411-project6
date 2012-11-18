/*
 * Hashtable implementation with linear probing for collision prevention
 */

#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

/* 
 * Hash table is of type hashtable_t.
 * User of hash table is unaware of it's internal structure. 
 */
typedef struct hashtable* hashtable_t;

/*
 * Hash and equals functions of a hash table
 */
typedef unsigned short (*hashFunc)(void*);
typedef unsigned short (*equalsFunc)(void*, void*);

/*
 * Initialize the table with a size and hashFunc hash and equalsFunc equals.
 * Returns NULL on failure.
 */
hashtable_t hashtable_init(unsigned int size, hashFunc hash, equalsFunc equals);

/*
 * Populate value for given key.
 * Return 0(success) or -1(failure)
 */
int hashtable_get(hashtable_t tbl, void *key, void **value);

/*
 * Remove the value stored under key. 
 * Returns key.
 */
void *hashtable_remove(hashtable_t hashtable, void *key);

/*
 * Put a key, value pair onto the table.
 * Return 0(success) or -1(failure)
 */
int hashtable_put(hashtable_t hashtable, void *key, void *value);

/*
 * Return size of hash table or -1(failure)
 */
int hashtable_size(hashtable_t hashtable);

/*
 * Destroy the hash table. It is left to the application programmer to free the value 
 * stored under the keys.
 */
void hashtable_destroy(hashtable_t tbl);

#endif __HASHTABLE_H__