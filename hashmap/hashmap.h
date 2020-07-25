// hashmap.h
// A concurrent, chaining hashmap.

#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdint.h>
#include <stdbool.h>

#include <pthread.h>

typedef uint64_t hash_t;

typedef bool   (*comparator_f) (void*, void*);
typedef void   (*deleter_f)    (void*, void*);
typedef hash_t (*hasher_f)     (void*);

typedef struct bucket_elem
{
    struct bucket_elem* next;

    // memoized hash value
    hash_t hash;

    void* key;    // inserted key
    void* value;  // inserted value
} bucket_elem_t;

typedef struct bucket
{
    bucket_elem_t*   first;
    pthread_rwlock_t lock;
} bucket_t;

typedef struct concurrent_hashmap
{
    comparator_f comparator; // key comparator
    deleter_f    deleter;    // key/value deleter
    hasher_f     hasher;     // key hasher

    // total count of items in map
    size_t n_items;

    // dynamic array of buckets
    bucket_t* buckets;
    size_t    n_buckets;
} concurrent_hashmap_t;

// map_new()
//
// Construct a new map.
//
// Returns:
//  pointer to newly initialized map
//  NULL on failure
concurrent_hashmap_t* map_new(
    comparator_f comparator,
    deleter_f    deleter,
    hasher_f     hasher);

// map_delete()
//
// Destroy an existing map.
void map_delete(concurrent_hashmap_t* map);

// map_insert()
//
// Insert a new element into the map.
//
// This insertion operation is restricted to
// inserting NEW elements into the map. That is,
// if an element exists in the map with a key 
// that is equivalent (as determined by comparator)
// to the key passed as an argument to this function,
// the new element will not be inserted, the out
// parameter "existing" is set to point to the 
// existing value for the specified key, and false
// is returned.
//
// Returns:
//  true on successful insertion of new element
//  false on failed insertion
void* map_insert(
    concurrent_hashmap_t* map, 
    void* key, 
    void* value,
    void* existing);

// map_update()
//
// Update the value associated with the specified key.
//
// This update operation is restricted to updating
// EXISTING elements in the map. That is, if an 
// element with the specified key does not exist
// in the map at the time this function is invoked,
// the element is NOT inserted into the map, and NULL
// is returned.
//
// Returns:
//  pointer to previous value on successful update
//  NULL on failed update
void* map_update(
    concurrent_hashmap_t* map,
    void* key,
    void* value);

// map_remove()
// Remove an existing item from the map.
//
// Returns:
//  true if the element with specified key is removed
//  false otherwise
bool map_remove(
    concurrent_hashmap_t* map, 
    void* key);

// map_find()
// Search the map for the specified key.
//
// Returns:
//  pointer to the key on success
//  NULL if key not present in map
void* map_find(
    concurrent_hashmap_t* map, 
    void* key);

// map_contains()
// Determine if the map contains the specified key.
//
// Returns:
//  true if map contains the specified key
//  false otherwise
bool map_contains(concurrent_hashmap_t* map, void* key);

#endif  // CONCURRENT_HASHMAP_H