// hashmap_attr.h
// Hashmap attribute specification. 

#ifndef HASHMAP_ATTR_H
#define HASHMAP_ATTR_H

#include <stddef.h>
#include <stdbool.h>

// The signature for a user-provided comparison function.
// This function is used to compare keys in the map for equality.
typedef bool (*comparator_f)(void*, void*);

// The signature for a user-provided key-length function.
// This function is used to determine the length of the
// data that is utilized as a key into the map.
typedef size_t (*keylen_f)(void*);

// The signature for a user provided delete function.
// This function is utilized internally by the hashmap
// in delete() operations ONLY in order to destroy
// the keys that are stored in the map.
typedef void (*key_deleter_f)(void*);

// The signature for a user provided delete function.
// This function is utilized internally by the hashmap
// in remove() and delete() operations in order to destroy
// the values that are stored in the map.
typedef void (*value_deleter_f)(void*);

typedef struct hashmap_attr
{
    float           load_factor;
    bool            key_is_literal;
    comparator_f    comparator;
    keylen_f        keylen;
    key_deleter_f   key_deleter;
    value_deleter_f value_deleter;
} hashmap_attr_t;

// hashmap_attr_new()
//
// Construct a new attributes instance.
//
// The members of the returned attributes instance 
// are default-initialized to invalid values. When
// this function is used to construct an attributes
// structure that will be used to construct a new 
// hashmap, ALL members of the attributes structure 
// must be set by the user prior to map construction,
// otherwise construction of the map will fail.
hashmap_attr_t* hashmap_attr_new(void);

// hashmap_attr_default()
//
// Construct a new attributes instance with valid
// defaults for all of the attribute members.
hashmap_attr_t* hashmap_attr_default(void);

// hashmap_attr_destroy()
//
// Destroy an attributes instance.
void hashmap_attr_delete(hashmap_attr_t* attr);

#endif // HASHMAP_ATTR_H