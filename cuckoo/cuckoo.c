// cuckoo.c
// A hashmap implementation utilizing the cuckoo hashing scheme.

#include "cuckoo.h"
#include "murmur3.h"

#include <assert.h>
#include <stdlib.h>

// ----------------------------------------------------------------------------
// Internal Declarations

// The initial number of slots in each internal table.
static const size_t INITIAL_TABLE_CAPACITY = 16;

// This implementation is hardcoded to utilize two 
// tables internally, although this is obviously not required.
static const size_t N_TABLES = 2;

// Every slot in each table contains a key and a value.
typedef struct slot
{
    key_t key;
    void* value;
} slot_t;

// Internally, every table in the map is 
// represented as a contiguous array of slots.
typedef slot_t* table_t;

struct cuckoo_map
{
    // The internal tables.
    table_t* tables;
    // The current size of each table.
    size_t table_capacity;

    // The user-provided delete function.
    deleter_f deleter;

    // The number of table resize operations performed. 
    size_t n_resize;
    // The total number of items currently in the map.
    size_t n_items;
};

static uint32_t get_hash(key_t key, uint32_t seed);

static bool insert_with_evictions(
    table_t* tables, 
    size_t   table_capacity, 
    key_t    key, 
    void*    value);
static bool insert_into_free_slot(
    table_t* tables, 
    size_t   capacity,
    key_t    key, 
    void*    value);
static void swap_slot(
    key_t* new_key, 
    void** new_value, 
    key_t* old_key, 
    void** old_value);

static bool resize_map(cuckoo_map_t* map);

static table_t* construct_tables(const size_t capacity);
static void initialize_table(table_t table, size_t capacity);

static void destroy_tables(cuckoo_map_t* map);
static void destroy_table(table_t table, size_t capacity, deleter_f deleter);

// ----------------------------------------------------------------------------
// Exported

cuckoo_map_t* cuckoo_new(deleter_f deleter)
{
    if (NULL == deleter)
    {
        return NULL;
    }

    cuckoo_map_t* map = malloc(sizeof(cuckoo_map_t));
    if (NULL == map)
    {
        return NULL;
    }

    table_t* tables = construct_tables(INITIAL_TABLE_CAPACITY);
    if (NULL == tables)
    {
        free(map);
        return NULL;
    }

    // successfully performed all allocations 
    map->tables         = tables;
    map->table_capacity = INITIAL_TABLE_CAPACITY;
    
    map->deleter = deleter;

    map->n_resize = 0;
    map->n_items  = 0;

    return map;
}

void cuckoo_delete(cuckoo_map_t* map)
{
    if (NULL == map)
    {
        return;
    }

    destroy_tables(map);
    free(map);
}

bool cuckoo_insert(
    cuckoo_map_t* map, 
    key_t key, 
    void* value, 
    void** out)
{
    if (NULL == map || 0 == key)
    {
        return false;
    }

    if (out != NULL)
    {
        *out = NULL;
    }

    // try to insert into both tables
    for (size_t i = 0; i < N_TABLES; ++i)
    {
        const uint32_t hash  = get_hash(key, i);
        const uint32_t index = hash & (map->table_capacity - 1);

        slot_t slot = map->tables[i][index];

        if (0 == slot.key)
        {
            // found an empty slot, just insert and get out
            slot.key   = key;
            slot.value = value;
            return true;
        }

        if (key == slot.key)
        {
            // key collision; remove the old value and insert updated value
            if (out != NULL)
            {
                *out = slot.value;
            }
            else
            {
                map->deleter(slot.value);
            }

            slot.value = value;
            return true;
        }
    }

    // the index to which this key hashed in each table
    // is occupied by a non-duplicate key; need to evict
    while (!insert_with_evictions(map->tables, map->table_capacity, key, value))
    {
        // eviction until resolution may still fail in 
        // the event that a cycle of eviction is encountered,
        // in which case we need to perform a resize of the map
        assert(resize_map(map));
        map->n_resize++;
    }

    map->n_items++;

    return true;
}

void* cuckoo_find(cuckoo_map_t* map, key_t key)
{
    if (NULL == map || 0 == key)
    {
        return NULL;
    }

    for (size_t i = 0; i < N_TABLES; ++i)
    {
        const uint32_t hash  = get_hash(key, i);
        const uint32_t index = hash & (map->table_capacity - 1);

        slot_t slot = map->tables[i][index];
        if (slot.key == key)
        {
            return slot.value;
        }
    }

    // not found
    return NULL;
}

bool cuckoo_remove(cuckoo_map_t* map, key_t key)
{
    if (NULL == map || 0 == key)
    {
        return false;
    }

    bool found = false;

    for (size_t i = 0; i < N_TABLES; ++i)
    {
        const uint32_t hash  = get_hash(key, i);
        const uint32_t index = hash & (map->table_capacity - 1);

        slot_t slot = map->tables[i][index];
        if (slot.key == key)
        {
            map->deleter(slot.value);
            map->n_items--;

            slot.key   = 0;
            slot.value = NULL;

            found = true;
            break;
        }
    }   

    return found; 
}

bool cuckoo_contains(cuckoo_map_t* map, key_t key)
{
    return cuckoo_find(map, key) != NULL;
}

// ----------------------------------------------------------------------------
// Internal

// compute the hash for a given key and seed
static uint32_t get_hash(key_t key, uint32_t seed)
{
    unsigned char buffer[4];
    MurmurHash3_x86_32((void*)key, sizeof(key_t), seed, buffer);
    return (*(uint32_t*)buffer);
}

// attempt to insert key into map, evicting keys as necessary,
// until conflicts are resolved or cycle is encountered
static bool insert_with_evictions(
    table_t* tables, 
    size_t   table_capacity,
    key_t    key, 
    void*    value)
{
    // NOTE: there a variety of heuristics used to determine
    // when a rehash operation is required when utilizing a
    // cuckoo hashing scheme; in production systems, it is far
    // more popular to choose some relatively long chain length
    // and simply trigger a rehash in the event that a collision
    // chain of this length is encountered. Here, I opt for 
    // the "textbook" approach and only trigger a rehash when
    // a true cycle of collisions is encountered. As the math
    // demonstrates, we may be sure that such a cycle is present
    // when we encounter the initial key to be inserted for a 
    // third time during search for a free slot.

    const key_t init_key = key;  // the initial key
    size_t n_encountered = 0;    // the number of times the initial key has been seen

    key_t current_key = key;
    void* current_val = value;

    // the table selector for cases in which we need 
    // to evict and reinsert into the other table
    size_t table_idx = 0;

    // loop while there remains an unresolved key
    for (;;)
    {
        if (current_key == init_key)
        {
            if (n_encountered++ >= 3)
            {
                // cycle encountered, need to rehash
                return false;
            }
        }
    
        if (insert_into_free_slot(tables, table_capacity, key, value))
        {
            // found a free slot into which to insert the key; done
            break;
        }

        // otherwise, no free slots for key, need to evict and start again

        // compute the hash and index for the current key
        const uint32_t hash  = get_hash(current_key, table_idx);
        const uint32_t index = hash & (table_capacity - 1);

        // and swap the new key and value into the evicted slot
        slot_t slot = tables[table_idx][index];
        swap_slot(&current_key, &current_val, 
            &slot.key, &slot.value);

        // update the table index for next iteration
        table_idx = table_idx ^ 1;
    }

    return true;
}

static bool insert_into_free_slot(
    table_t* tables, 
    size_t   capacity,
    key_t    key, 
    void*    value)
{
    for (size_t i = 0; i < N_TABLES; ++i)
    {
        const uint32_t hash  = get_hash(key, i);
        const uint32_t index = hash & (capacity - 1);

        slot_t slot = tables[i][index]; 

        if (0 == slot.key)
        {
            slot.key   = key;
            slot.value = value;
            return true;
        }
    }

    // no free slot found for key
    return false;
}

static void swap_slot(
    key_t* new_key, 
    void** new_value, 
    key_t* old_key, 
    void** old_value)
{
    uint32_t tmp_key = *old_key;
    void* tmp_val    = *old_value;

    *old_key   = *new_key;
    *old_value = *new_value;

    *new_key   = tmp_key;
    *new_value = tmp_val;
}

// resize the entire map by expanding table capacity
static bool resize_map(cuckoo_map_t* map)
{
    // common heuristic: double table size
    const size_t new_capacity = map->table_capacity << 1;

    // construct the new tables
    table_t* new_tables = construct_tables(new_capacity);
    if (NULL == new_tables)
    {
        return false;
    }

    // insert every item currently in 
    // the tables into the new set of tables
    for (size_t i = 0; i < N_TABLES; ++i)
    {
        for (size_t j = 0; j < map->table_capacity; ++j)
        {
            slot_t slot = map->tables[i][j];
            if (slot.key != 0)
            {
                const bool r 
                    = insert_with_evictions(new_tables, new_capacity, slot.key, slot.value);
                assert(r);
            }
        }
    }

    destroy_tables(map);

    map->tables         = new_tables;
    map->table_capacity = new_capacity;

    return true;
}

static table_t* construct_tables(const size_t capacity)
{
    // allocate the space for table heads
    table_t* tables = calloc(N_TABLES, sizeof(table_t));
    if (NULL == tables)
    {
        return NULL;
    }

    // allocate space for the tables themselves
    tables[0] = calloc(capacity, sizeof(slot_t));
    tables[1] = calloc(capacity, sizeof(slot_t));

    if (NULL == tables[0] || NULL == tables[1])
    {
        destroy_table(tables[0], capacity, NULL);
        destroy_table(tables[1], capacity, NULL);
        free(tables);
        return NULL;
    }

    for (size_t i = 0; i < N_TABLES; ++i)
    {
        initialize_table(tables[i], capacity);
    }

    return tables;
}

static void initialize_table(table_t table, size_t capacity)
{
    for (size_t i = 0; i < capacity; ++i)
    {
        table[i].key   = 0;
        table[i].value = NULL;
    }
}

static void destroy_tables(cuckoo_map_t* map)
{
    for (size_t i = 0; i < N_TABLES; ++i)
    {
        destroy_table(map->tables[i], map->table_capacity, map->deleter);
    }

    free(map->tables);
}

static void destroy_table(table_t table, size_t capacity, deleter_f deleter)
{
    if (NULL == table)
    {
        return;
    }

    if (deleter != NULL)
    {
        for (size_t i = 0; i < capacity; ++i)
        {
            // destroy every value in the table

            if (table[i].value != NULL)
            {
                deleter(table[i].value);
            }
        }
    }

    free(table);
}