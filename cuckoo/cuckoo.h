// cuckoo.h
// A hashmap implementation utilizing the cuckoo hashing scheme.

#ifndef CUCKOO_H
#define CUCKOO_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct cuckoo_map cuckoo_map_t;

// To simplify things, we limit the key type to 64-bit integers.
typedef uint64_t key_t;

// The signature of the user-provided delete function.
typedef void (*deleter_f)(void*);

cuckoo_map_t* cuckoo_new(deleter_f deleter);

void cuckoo_delete(cuckoo_map_t* map);

bool cuckoo_insert(cuckoo_map_t* map, key_t key, void* value, void** out);

void* cuckoo_find(cuckoo_map_t* map, key_t key);

bool cuckoo_remove(cuckoo_map_t* map, key_t key);

bool cuckoo_contains(cuckoo_map_t* map, key_t key);

#endif // CUCKOO_H