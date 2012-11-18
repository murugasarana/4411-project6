#include <stdio.h>
#include <assert.h>

#include "hashtable.h"

int equals(void *a, void *b) {
    int _a = *((int *) a);
    int _b = *((int *) b);
    return _a == _b;
}

int hash(void *a) {
    int _a = *((int *) a);
    return _a;
}

void
test_saturation() {
    hashtable_t table = hashtable_init(2, hash, equals);
    int a,b,c;
    int _1,_2,_3;
    a = _1 = 1;
    b = _2 = 2;
    c = _3 = 3;
    assert(hashtable_put(table, &a, &_1) == 0);
    assert(hashtable_put(table, &b, &_2) == 0);
    assert(hashtable_put(table, &c, &_3) == -1);
    hashtable_free(table);
}

void
test_get() {
    hashtable_t table = hashtable_init(3, hash, equals);
    int a,b,c,d;
    int _1,_2,_3;
    int *temp;
    a = _1 = 1;
    b = _2 = 2;
    c = _3 = 3;
    d = 4;
    assert(hashtable_put(table, &a, &_1) == 0);
    assert(hashtable_put(table, &b, &_2) == 0);
    assert(hashtable_put(table, &c, &_3) == 0);
    
    assert(hashtable_get(table, &a, &temp) == 0);
    assert(*temp == _1);
    
    assert(hashtable_get(table, &b, &temp) == 0);
    assert(*temp == _2);
    
    assert(hashtable_get(table, &c, &temp) == 0);
    assert(*temp == _3);
    
    assert(hashtable_get(table, &d, &temp) == -1);
    assert(temp == NULL);
    hashtable_free(table);
}

void
test_collision() {
    hashtable_t table = hashtable_init(3, hash, equals);
    int a,b,c,d;
    int _1,_2,_3;
    int *temp;
    a = _1 = 1;
    b = 4;
    c = 7;
    _2 = 2;
    _3 = 3;
    d = 2;
    assert(hashtable_put(table, &a, &_1) == 0);
    assert(hashtable_put(table, &b, &_2) == 0);
    assert(hashtable_put(table, &c, &_3) == 0);
    
    assert(hashtable_get(table, &a, &temp) == 0);
    assert(*temp == _1);
    
    assert(hashtable_get(table, &b, &temp) == 0);
    assert(*temp == _2);
    
    assert(hashtable_get(table, &c, &temp) == 0);
    assert(*temp == _3);
    
    assert(hashtable_get(table, &d, &temp) == -1);
    assert(temp == NULL);
    hashtable_free(table);
}

void
test_remove() {
    hashtable_t table = hashtable_init(3, hash, equals);
    int a,b,c,d;
    int _1,_2,_3;
    int *temp;
    a = _1 = 1;
    b = 4;
    c = 7;
    _2 = 2;
    _3 = 3;
    d = 2;
    assert(hashtable_put(table, &a, &_1) == 0);
    assert(hashtable_put(table, &b, &_2) == 0);
    assert(hashtable_put(table, &c, &_3) == 0);
    
    assert(hashtable_get(table, &a, &temp) == 0);
    assert(*temp == _1);
    
    assert(hashtable_get(table, &b, &temp) == 0);
    assert(*temp == _2);
    
    assert(hashtable_get(table, &c, &temp) == 0);
    assert(*temp == _3);
    
    assert(hashtable_get(table, &d, &temp) == -1);
    assert(temp == NULL);
    
    hashtable_remove(table, &a);
    assert(hashtable_get(table, &a, &temp) == -1);
    assert(temp == NULL);
    
    hashtable_free(table);
}

int
main() {
    printf("Testing hash table operations...\n");
    
    printf("Testing hash table saturation...\n");
    test_saturation();
    
    printf("Testing hash table retrieval...\n");
    test_get();
    
    printf("Testing hash collisions...\n");
    test_collision();
    
    printf("Testing done!\n");
    return 0;
}