// flat_map.h
// A concurrent open-addressing hashmap utilizing 
// linear probing and configurable locking granularity.

#ifndef FLAT_MAP_H
#define FLAT_MAP_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct flat_map flat_map_t;

// We restrict the key type to 64-bit integers.
typedef uint64_t map_key_t;

// The signature for user-provided delete function.
typedef void (*deleter_f)(void*);

// flat_map_new()
//
// Construct a new map instance.
//
// Arguments:
//  page_size - the number of map items in a single `page`,
//              all of which are protected by the same lock;
//              this parameter determines the effective level 
//              of concurrency supported by the map
//  deleter   - the user-provided delete function used to 
//              destroy values stored in the map
//
// Returns:
//  A pointer to the newly constructed map instance on success
//  NULL on failure
flat_map_t* flat_map_new(
    size_t page_size, 
    deleter_f deleter);

// flat_map_delete()
//
// Destroy an existing map instance.
//
// This function is not reentrant; multiple threads
// should not call this function concurrently on the
// same map instance.
//
// Once this function completes, all of the values 
// stored in the map are destroyed, along with the 
// map itself. Subsequent access to the values stored
// in the map through external pointers to this data
// are undefined behavior.
//
// Arguments:
//  map - pointer to an existing map instance
void flat_map_delete(flat_map_t* map);

// flat_map_insert()
//
// Insert a new key / value pair into the map.
//
// This function is reentrant and may be invoked
// from multiple threads of execution concurrently.
//
// The semantics of this map are such that it does
// not support insertion of duplicate keys. Therefore,
// the insertion algorithm proceeds as follows:
//  
//  - If `key` is already present in the map, the 
//    value associated with `key` is updated to `value`
//    and, if `out` is non-NULL, the old value associated
//    with `key` is returned via `out`
//  - Otherwise, `key` is inserted into the map with 
//    the associated value `value`
//
// This operation may trigger a map resize operation.=
//
// Arguments:
//  map   - pointer to an existing map instance
//  key   - the key under which to insert `value`
//  value - the value to insert
//  out   - the out parameter through which an 
//          existing value under `key` is returned
//
// Returns:
//  `true` if the value was inserted into the map
//  `false` otherwise
bool flat_map_insert(
    flat_map_t* map, 
    map_key_t   key, 
    void*       value, 
    void**      out);

// flat_map_remove()
//
// Remove a key / value association from the map.
//
// This function is reentrant and may be invoked
// from multiple threads of execution concurrently.
//
// If `key` is present in the map, the value associated
// with `key` is destroyed via the delete function provided
// in the constructor of the flat map instance, and `true`
// is returned. Otherwise, `key` is not present in the map,
// and `false` is returned.
//
// Arguments:
//  map - pointer to an existing map instance
//  key - the key for the key / value association to remove
//
// Returns:
//  `true` if the key / value association identified by `key` is removed
//  `false` otherwise 
bool flat_map_remove(flat_map_t* map, map_key_t key);

// flat_map_find()
//
// Search the map instance for a key / value association.
//
// This function is reentrant and may be invoked
// from multiple threads of execution concurrently.
//
// Arguments:
//  map - pointer to an existing map instance
//  key - the key that identifies the association for which to search
//
// Returns:
//  A pointer to the value associated with `key` if present in the map
//  NULL otherwise
void* flat_map_find(flat_map_t* map, map_key_t key);

// flat_map_contains()
//
// Determine if the map contains a key / value association.
//
// This function is reentrant and may be invoked
// from multiple threads of execution concurrently.
//
// Arguments:
//  map - pointer to an existing map instance
//  key - the key that identifies the association to query
//
// Returns:
//  `true` if `key` is present in the map
//  `false` otherwise
bool flat_map_contains(flat_map_t* map, map_key_t key);

#endif