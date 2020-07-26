// hashmap.c
// A concurrent, chaining hashmap.

#define _GNU_SOURCE

#include "murmur3.h"
#include "hashmap.h"
#include "intrusive_list.h"

#include <stdlib.h>
#include <pthread.h>

// The output of the hash function used internally.
typedef uint32_t hash_t;

// The top-level lock used to exclude entry during resize.
typedef pthread_rwlock_t map_lock_t;

// The initial number of buckets in internal bucket array.
static const size_t INITIAL_N_BUCKETS = 4;

typedef struct bucket_iter_ctx
{
    void*        query_key;
    comparator_f comparator;
} bucket_iter_ctx_t;

// Each key / value association in the table is stored in a bucket 
// element that is a member of an intrusive linked-list of buckets.
typedef struct bucket_item
{
    // The entry in the intrusive linked-list.
    list_entry_t entry;

    // The memoized hash value for the key.
    hash_t hash;

    void* key;    // Inserted key
    void* value;  // Inserted value
} bucket_item_t;

// Internally, the map utilizes a contiguous array of buckets to
// stored key / value associations. Each bucket is an intrusive
// linked-list of items protected by a reader / writer lock.
typedef struct bucket
{
    list_entry_t     head;
    pthread_rwlock_t lock;
} bucket_t;

struct hashmap
{
    // The top-level map lock.
    map_lock_t map_lock;

    // The maximum load factor.
    float load_factor;

    // User-provided attribute functions.
    comparator_f    comparator;     // key comparator

    bool            key_is_literal;
    keylen_f        keylen;         // key length computer

    key_deleter_f   key_deleter;    // key deleter
    value_deleter_f value_deleter;  // value deleter

    // The total count of items in the map.
    size_t n_items;

    // The dynamic array of buckets.
    bucket_t* buckets;
    // The number of buckets currently in the array.
    size_t    n_buckets;
};

// ----------------------------------------------------------------------------
// Internal Prototypes: Map Initialization

static void initialize_map_lock(map_lock_t* lock);
static void destroy_map_lock(map_lock_t* lock);

static void lock_map_rw(hashmap_t* map);
static void lock_map_resize(hashmap_t* map);
static void unlock_map(hashmap_t* map);

// ----------------------------------------------------------------------------
// Internal Prototypes: Bucket Operations

static bucket_t* new_buckets(size_t n_buckets);
static void initialize_bucket(bucket_t* bucket);
static void deinitialize_bucket(bucket_t* bucket);

static void destroy_buckets(
    bucket_t*       buckets, 
    size_t          n_buckets, 
    key_deleter_f   key_deleter, 
    value_deleter_f value_deleter);
static void flush_bucket(
    bucket_t*       bucket, 
    key_deleter_f   key_deleter, 
    value_deleter_f value_deleter);

static void lock_bucket_read(bucket_t* bucket);
static void lock_bucket_write(bucket_t* bucket);
static void unlock_bucket(bucket_t* bucket);

static bucket_item_t* new_bucket_item(hash_t hash, void* key, void* value);
static void destroy_bucket_item(bucket_item_t* item);

static bucket_item_t* bucket_find_by_key(
    hashmap_t* map, 
    bucket_t*  bucket,
    void*      key);
static bool bucket_finder(list_entry_t* entry, void* ctx);

static void insert_into_bucket(
    bucket_t*      bucket, 
    bucket_item_t* item);
static void remove_from_bucket(
    bucket_t*      bucket, 
    bucket_item_t* item);

// ----------------------------------------------------------------------------
// Internal Prototypes: Resize

static void resize_map(hashmap_t* map);

// ----------------------------------------------------------------------------
// Internal Prototypes: Atomic Wrappers

static inline size_t atomic_increment(size_t* n);
static inline size_t atomic_decrement(size_t* n);
static inline size_t atomic_load(size_t* n);
static inline void atomic_store(size_t* ptr, size_t* n);

// ----------------------------------------------------------------------------
// Internal Prototypes: General Utility

static hash_t hash_key(
    void*    key, 
    keylen_f keylen, 
    bool     key_is_literal);

static size_t bucket_index(
    const hash_t hash, 
    const size_t n_buckets);

static bool need_resize(
    const size_t n_items, 
    const size_t n_buckets,
    const float load_factor);

// ----------------------------------------------------------------------------
// Exported

hashmap_t* hashmap_new(void)
{
    hashmap_attr_t* default_attr = hashmap_attr_default();
    if (NULL == default_attr)
    {
        return NULL;
    }

    return hashmap_new_with_attr(default_attr);
}

hashmap_t* hashmap_new_with_attr(hashmap_attr_t* attr)
{
    if (NULL == attr 
    || attr->load_factor == 0.0f 
    || NULL == attr->comparator 
    || NULL == attr->keylen 
    || NULL == attr->key_deleter 
    || NULL == attr->value_deleter)
    {
        return NULL;
    }

    hashmap_t* map = malloc(sizeof(hashmap_t));
    if (NULL == map)
    {
        return NULL;
    }

    bucket_t* buckets = new_buckets(INITIAL_N_BUCKETS);
    if (NULL == buckets)
    {
        free(map);
        return NULL;
    }

    initialize_map_lock(&map->map_lock);

    map->buckets   = buckets;
    map->n_buckets = INITIAL_N_BUCKETS;

    map->load_factor    = attr->load_factor;
    map->comparator     = attr->comparator;
    map->key_is_literal = attr->key_is_literal;
    map->keylen         = attr->keylen;
    map->key_deleter    = attr->key_deleter;
    map->value_deleter  = attr->value_deleter;

    map->n_items = 0;

    return map;
}

void hashmap_delete(hashmap_t* map)
{
    if (NULL == map)
    {
        return;
    }

    destroy_map_lock(&map->map_lock);

    destroy_buckets(
        map->buckets, 
        map->n_buckets, 
        map->key_deleter, 
        map->value_deleter);

    free(map);
}

bool hashmap_insert(
    hashmap_t* map, 
    void*      key, 
    void*      value,
    void**     replaced)
{
    if (NULL == map)
    {
        return false;
    }

    if (replaced != NULL)
    {
        *replaced = NULL;
    }

    lock_map_rw(map);

    const size_t new_n_items = atomic_load(&map->n_items) + 1;
    if (need_resize(new_n_items, map->n_buckets, map->load_factor))
    {
        unlock_map(map);

        resize_map(map);

        lock_map_rw(map);
    }

    bool inserted = false;

    // compute the hash for the key
    const hash_t hash  = hash_key(key, map->keylen, map->key_is_literal);

    // locate the appropriate bucket
    bucket_t* bucket = &map->buckets[bucket_index(hash, map->n_buckets)];
    
    // lock the bucket for writing 
    lock_bucket_write(bucket);

    // search the bucket for the key
    bucket_item_t* item = bucket_find_by_key(map, bucket, key);
    if (NULL == item)
    {
        // key not present in the map; insert a new item
        bucket_item_t* new_item = new_bucket_item(hash, key, value);
        if (new_item != NULL)
        {
            insert_into_bucket(bucket, new_item);
            inserted = true;
        }
    }
    else
    {
        // key already exists in the map, replace the current value

        if (replaced != NULL)
        {
            // return the existing value
            *replaced = item->value;
        }
        else
        {
            // otherwise, destroy it to prevent leak
            map->value_deleter(item->value);
        }
        
        item->value = value;
        inserted = true;
    }

    unlock_bucket(bucket);

    atomic_increment(&map->n_items);

    unlock_map(map);

    return inserted;
}

bool hashmap_remove(hashmap_t* map, void* key)
{
    if (NULL == map)
    {
        return false;
    }

    // lock the map for read / write
    lock_map_rw(map);

    // compute the hash for the key
    const hash_t hash = hash_key(key, map->keylen, map->key_is_literal);

    // locate the appropriate bucket
    bucket_t* bucket = &map->buckets[bucket_index(hash, map->n_buckets)];

    // lock the bucket for writing
    lock_bucket_write(bucket);

    // search the bucket for the key
    bucket_item_t* item = bucket_find_by_key(map, bucket, key);
    if (item != NULL)
    {
        // located a matching key, destroy the associated value
        map->value_deleter(item->value);

        // remove the item from the bucket
        remove_from_bucket(bucket, item);

        // and destroy the item
        destroy_bucket_item(item);
    }

    unlock_bucket(bucket);

    atomic_decrement(&map->n_items);

    unlock_map(map);

    return item != NULL;
}

void* hashmap_find(hashmap_t* map, void* key)
{
    if (NULL == map)
    {
        return NULL;
    }

    // lock the map for read / write
    lock_map_rw(map);

    // compute the hash for the key
    const hash_t hash = hash_key(key, map->keylen, map->key_is_literal);

    // locate the appropriate bucket
    bucket_t* bucket = &map->buckets[bucket_index(hash, map->n_buckets)];

    // lock the bucket for reading
    lock_bucket_read(bucket);

    // search the bucket for the key
    bucket_item_t* item = bucket_find_by_key(map, bucket, key);
    void* value = (NULL == item) ? NULL : item->value;
    
    unlock_bucket(bucket);
    unlock_map(map);

    return value;
}


bool hashmap_contains(hashmap_t* map, void* key)
{
    return hashmap_find(map, key) != NULL;
}

// ----------------------------------------------------------------------------
// Internal: Map Initialization 

// Initialize the top-level map lock.
static void initialize_map_lock(map_lock_t* lock)
{
    pthread_rwlockattr_t attr;
    pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NP);
    pthread_rwlock_init(lock, &attr);
}

// Destroy the top-level map lock.
static void destroy_map_lock(map_lock_t* lock)
{
    pthread_rwlock_destroy(lock);
}

// Acquire shared access to the map for read / write operations. 
static void lock_map_rw(hashmap_t* map)
{
    pthread_rwlock_rdlock(&map->map_lock);
}

// Acquire exclusive access to the map for resize.
static void lock_map_resize(hashmap_t* map)
{
    pthread_rwlock_wrlock(&map->map_lock);
}

// Release access to the map.
static void unlock_map(hashmap_t* map)
{
    pthread_rwlock_unlock(&map->map_lock);
}

// ----------------------------------------------------------------------------
// Internal: Bucket Operations 

// Construct and initialize a new bucket array.
static bucket_t* new_buckets(size_t n_buckets)
{
    bucket_t* buckets = calloc(n_buckets, sizeof(bucket_t));
    if (NULL == buckets)
    {
        return NULL;
    }

    for (size_t i = 0; i < n_buckets; ++i)
    {
        initialize_bucket(&buckets[i]);
    }

    return buckets;
}

// Initialize a new bucket in the bucket array.
static void initialize_bucket(bucket_t* bucket)
{
    list_init(&bucket->head);
    pthread_rwlock_init(&bucket->lock, NULL);
}

// Destroy the lock embedded in the bucket.
static void deinitialize_bucket(bucket_t* bucket)
{
    pthread_rwlock_destroy(&bucket->lock);
}

// Destroy an entire bucket array.
static void destroy_buckets(
    bucket_t*       buckets, 
    size_t          n_buckets, 
    key_deleter_f   key_deleter, 
    value_deleter_f value_deleter)
{
    for (size_t i = 0; i < n_buckets; ++i)
    {
        bucket_t* bucket = &buckets[i];
        flush_bucket(bucket, key_deleter, value_deleter);
        deinitialize_bucket(bucket);
    }

    free(buckets);
}

// Flush and destroy all items stored in an individual bucket.
static void flush_bucket(
    bucket_t*       bucket, 
    key_deleter_f   key_deleter, 
    value_deleter_f value_deleter)
{
    list_entry_t* current;
    while ((current = list_pop_front(&bucket->head)) != NULL)
    {
        bucket_item_t* item = (bucket_item_t*) current;
        
        // if the key deleter is provided, destroy the stored key
        if (key_deleter != NULL)
        {
            key_deleter(item->key);
        }

        // if the value deleter is provided, destroy the stored value
        if (value_deleter != NULL)
        {
            value_deleter(item->value);
        }

        // destroy the item itself
        destroy_bucket_item(item);
    }
}

// Acquire shared access to the bucket.
static void lock_bucket_read(bucket_t* bucket)
{
    pthread_rwlock_rdlock(&bucket->lock);
}

// Acquire exclusive access to the bucket.
static void lock_bucket_write(bucket_t* bucket)
{
    pthread_rwlock_wrlock(&bucket->lock);
}

// Release access to the bucket.
static void unlock_bucket(bucket_t* bucket)
{
    pthread_rwlock_unlock(&bucket->lock);
}

// Construct and initialize a new bucket item.
static bucket_item_t* new_bucket_item(hash_t hash, void* key, void* value)
{
    bucket_item_t* item = malloc(sizeof(bucket_item_t*));
    if (NULL == item)
    {
        return NULL;
    }

    item->hash  = hash;
    item->key   = key;
    item->value = value;

    return item;
}

// Destroy (deallocate) a bucket item.
static void destroy_bucket_item(bucket_item_t* item)
{
    free(item);
}

static bucket_item_t* bucket_find_by_key(
    hashmap_t* map, 
    bucket_t*  bucket,
    void*      key)
{
    bucket_iter_ctx_t ctx = {
        .query_key  = key,
        .comparator = map->comparator
    };

    return (bucket_item_t*) list_find(&bucket->head, bucket_finder, &ctx);
}

static bool bucket_finder(list_entry_t* entry, void* ctx)
{
    bucket_item_t*     item     = (bucket_item_t*) entry;
    bucket_iter_ctx_t* iter_ctx = (bucket_iter_ctx_t*) ctx;

    return iter_ctx->comparator(item->key, iter_ctx->query_key);
}

// Insert the specified bucket item into `bucket`.
static void insert_into_bucket(
    bucket_t*      bucket, 
    bucket_item_t* item)
{
    list_push_front(&bucket->head, &item->entry);
}

// Remove the specified item from `bucket`. 
static void remove_from_bucket(
    bucket_t*      bucket, 
    bucket_item_t* item)
{
    list_remove_entry(&bucket->head, &item->entry);
}

// ----------------------------------------------------------------------------
// Internal Prototypes: Resize

static void resize_map(hashmap_t* map)
{
    lock_map_resize(map);

    if (!need_resize(map->n_items + 1, map->n_buckets, map->load_factor))
    {
        // we lost a race to perform the resize, abort
        unlock_map(map);
        return;
    }

    // we now have exclusive access to the entire map

    // double the capacity of the map on resize
    const size_t new_n_buckets = map->n_buckets << 1;

    // construct and initialize a new array of buckets
    bucket_t* buckets = new_buckets(new_n_buckets);

    // iterate over each bucket in existing array
    for (size_t i = 0; i < map->n_buckets; ++i)
    {
        bucket_t* bucket = &map->buckets[i];

        // iterate over each item in that bucket
        bucket_item_t* item;
        while ((item = (bucket_item_t*) list_pop_front(&bucket->head)) != NULL)
        {
            // compute the new index for the item
            const size_t new_index = bucket_index(item->hash, new_n_buckets);
            
            // locate the new bucket for the item
            bucket_t* new_bucket = &buckets[new_index];
            
            // insert the item into its new bucket
            insert_into_bucket(new_bucket, item);
        }

        // all of the items in the bucket have been moved;
        // just deinitialize the bucket to complete cleanup
        deinitialize_bucket(bucket);
    }

    bucket_t* old_buckets = map->buckets;

    map->n_buckets = new_n_buckets;
    map->buckets   = buckets;

    free(old_buckets);
    unlock_map(map);
}

// ----------------------------------------------------------------------------
// Internal: Atomic Wrappers

static inline size_t atomic_increment(size_t* n)
{
    return __atomic_add_fetch(n, 1, __ATOMIC_SEQ_CST);
}

static inline size_t atomic_decrement(size_t* n)
{
    return __atomic_sub_fetch(n, 1, __ATOMIC_SEQ_CST);
}

static inline size_t atomic_load(size_t* n)
{
    return __atomic_load_n(n, __ATOMIC_ACQUIRE);
}

static inline void atomic_store(size_t* ptr, size_t* n)
{
    __atomic_store_n(ptr, n, __ATOMIC_RELEASE);
}

// ----------------------------------------------------------------------------
// Internal: General Utility 

// Compute the hash for `key`.
static hash_t hash_key(
    void*    key, 
    keylen_f keylen, 
    bool     key_is_literal)
{
    unsigned char buffer[sizeof(hash_t)];
    void* to_hash = key_is_literal ? &key : key;

    MurmurHash3_x86_32(to_hash, keylen(key), 0, buffer);

    return (*(hash_t*)buffer);
}

// Compute the bucket index for `hash`.
static size_t bucket_index(
    const hash_t hash, 
    const size_t n_buckets)
{
    return hash & (n_buckets - 1);
}

// Determine if a map resize is required.
static bool need_resize(
    const size_t n_items, 
    const size_t n_buckets,
    const float load_factor)
{
    // a resize is required when the total number
    // of items in the map exceeds the product of 
    // the load factor and the current number of buckets
    return n_items > (load_factor*n_buckets);
}