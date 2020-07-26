// hashmap.h
// A concurrent, chaining hashmap.

#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdint.h>
#include <stdbool.h>

#include "hashmap_attr.h"

// The hashmap type.
typedef struct hashmap hashmap_t;

// hashmap_new()
//
// Construct a new map.
//
// Returns:
//  pointer to newly initialized map
//  NULL on failure
hashmap_t* hashmap_new(void);

// hashmap_new_with_attr()
//
// Construct a new map with attributes specified
// by the provided attributes structure.
hashmap_t* hashmap_new_with_attr(hashmap_attr_t* attr);

// map_delete()
//
// Destroy an existing map instance.
void hashmap_delete(hashmap_t* map);

// hashmap_insert()
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
//  `true` on successful insertion of new element
//  `false` on failed insertion
bool hashmap_insert(
    hashmap_t* map, 
    void*      key, 
    void*      value,
    void**     replaced);

// hashmap_remove()
//
// Remove an existing item from the map.
//
// Returns:
//  `true` if the element with specified key is removed
//  `false` otherwise
bool hashmap_remove(hashmap_t* map, void* key);

// hashmap_find()
//
// Search the map for the specified key.
//
// Returns:
//  A pointer to the key on success
//  NULL if key not present in map
void* hashmap_find(hashmap_t* map, void* key);

// hashmap_contains()
// Determine if the map contains the specified key.
//
// Returns:
//  `true` if map contains the specified key
//  `false` otherwise
bool hashmap_contains(hashmap_t* map, void* key);

#endif  // HASHMAP_H