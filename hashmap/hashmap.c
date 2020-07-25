// hashmap.c
// A concurrent, chaining hashmap.

#include "hashmap.h"

#include <stdlib.h>

// the initial number of hashtable buckets
#define INITIAL_NBUCKETS 4

// the default map load factor
#define DEFAULT_LOAD_FACTOR 1

// ----------------------------------------------------------------------------
// Internal Prototypes: Bucket Operations

static void bucket_init(bucket_t* bucket);
static void bucket_flush(bucket_t* bucket, deleter_f deleter);
static void bucket_lock_read(bucket_t* bucket);
static void bucket_lock_write(bucket_t* bucket);
static void bucket_unlock(bucket_t* bucket);

// ----------------------------------------------------------------------------
// Internal Prototypes: Atomic Wrappers

static inline size_t atomic_increment(size_t* n);
static inline size_t atomic_load(size_t* n);
static inline size_t atomic_store(size_t* ptr, size_t* n);

// ----------------------------------------------------------------------------
// Internal Prototypes: General Utility

static size_t bucket_idx(
    const hash_t hash, 
    const size_t n_buckets);

static bool need_resize(
    const size_t n_items, 
    const size_t n_buckets);

// ----------------------------------------------------------------------------
// Exported

concurrent_hashmap_t* map_new(
    comparator_f comparator,
    deleter_f    deleter,
    hasher_f     hasher)
{
    if (NULL == comparator 
     || NULL == deleter
     || NULL == hasher)
    {
        return NULL;
    }

    concurrent_hashmap_t* map = malloc(sizeof(concurrent_hashmap_t));
    if (NULL == map)
    {
        return NULL;
    }

    bucket_t* buckets = calloc(INITIAL_NBUCKETS, sizeof(bucket_t));
    if (NULL == buckets)
    {
        free(map);
        return NULL;
    }

    // initialize the buckets
    for (size_t i = 0; i < INITIAL_NBUCKETS; ++i)
    {
        bucket_init(&buckets[i]);
    }

    map->buckets   = buckets;
    map->n_buckets = INITIAL_NBUCKETS;

    map->comparator = comparator;
    map->deleter    = deleter;
    map->hasher     = hasher;

    return map;
}

void map_delete(concurrent_hashmap_t* map)
{
    if (NULL == map)
    {
        return;
    }

    bucket_t* buckets = map->buckets;
    for (size_t i = 0; i < map->n_buckets; ++i)
    {
        bucket_flush(&buckets[i], map->deleter);
    }

    free(buckets);
    free(map);
}

void* map_insert(
    concurrent_hashmap_t* map, 
    void* key, 
    void* value,
    void* existing
    )
{
    if (NULL == map
     || NULL == key
     || NULL == value
     || NULL == existing)
    {
        return NULL;
    }

    existing = NULL;

    const size_t n_items = atomic_increment(&map->n_items);
    if (need_resize(n_items, map->n_buckets))
    {
        // initiate a resize operation
    }

    return NULL;
}

void* map_update(
    concurrent_hashmap_t* map,
    void* key,
    void* value
    )
{
    if (NULL == map 
     || NULL == key
     || NULL == value)
    {
        return NULL;
    }

    return NULL;
}

bool map_remove(
    concurrent_hashmap_t* map, 
    void* key
    )
{
    if (NULL == map || NULL == key)
    {
        return false;
    }

    return true;
}

void* map_find(concurrent_hashmap_t* map, void* key)
{
    if (NULL == map
     || NULL == key)
    {
        return NULL;
    }

    const hash_t hash = map->hasher(key);

    const size_t n_buckets = atomic_load(&map->n_buckets);
    const size_t idx = bucket_idx(hash, n_buckets);
}

bool map_contains(concurrent_hashmap_t* map, void* key)
{
    return map_find(map, key) != NULL;
}

// ----------------------------------------------------------------------------
// Internal: Bucket Operations 

// bucket_init()
// Initialize a bucket.
static void bucket_init(bucket_t* bucket)
{
    bucket->first = NULL;
    pthread_rwlock_init(&bucket->lock, NULL);
}

// bucket_flush()
// Flush all elements from a bucket.
static void bucket_flush(bucket_t* bucket, deleter_f deleter)
{   
    // flush the elements of the bucket
    bucket_elem_t* current = bucket->first;
    while (current != NULL)
    {
        bucket_elem_t* tmp = current->next;
        
        deleter(current->key, current->value);
        free(current);
        
        current = tmp;
    }

    // destroy the bucket lock
    pthread_rwlock_destroy(&bucket->lock);
}

// bucket_lock_read()
// Acquire shared access to the bucket.
static void bucket_lock_read(bucket_t* bucket)
{
    pthread_rwlock_rdlock(&bucket->lock);
}

// bucket_lock_write()
// Acquire exclusive access to the bucket.
static void bucket_lock_write(bucket_t* bucket)
{
    pthread_rwlock_wrlock(&bucket->lock);
}

// bucket_unlock()
// Release access to the bucket.
static void bucket_unlock(bucket_t* bucket)
{
    pthread_rwlock_unlock(&bucket->lock);
}

// ----------------------------------------------------------------------------
// Internal: Atomic Wrappers

// atomic_increment()
static inline size_t atomic_increment(size_t* n)
{
    return __atomic_add_fetch(n, 1, __ATOMIC_SEQ_CST);
}

// atomic_load()
static inline size_t atomic_load(size_t* n)
{
    return __atomic_load_n(n, __ATOMIC_SEQ_CST);
}

// atomic_store()
static inline size_t atomic_store(size_t* ptr, size_t* n)
{
    __atomic_store_n(ptr, n, __ATOMIC_SEQ_CST);
}

// ----------------------------------------------------------------------------
// Internal: General Utility 

// bucket_idx()
static size_t bucket_idx(
    const hash_t hash, 
    const size_t n_buckets
    )
{
    const uint64_t as_uint64 = (uint64_t)hash;
    return as_uint64 % n_buckets;
}

// need_resize()
static bool need_resize(
    const size_t n_items, 
    const size_t n_buckets
    )
{
    // a resize is required when the total number
    // of items in the map exceeds the product of 
    // the load factor and the current number of buckets
    return n_items > (DEFAULT_LOAD_FACTOR*n_buckets);
}