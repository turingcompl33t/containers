// flat_map.c
// A concurrent open-addressing hashmap utilizing 
// linear probing and configurable locking granularity.

#define _GNU_SOURCE
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>

#include "murmur3.h"
#include "flat_map.h"

// ----------------------------------------------------------------------------
// Internal Declarations

// The key used to represent an empty cell.
static const uint32_t EMPTY_KEY     = 0;

// The key used to represent a tombstone cell.
static const uint32_t TOMBSTONE_KEY = ~0;

// The default maximum load factor for the table.
static const float LOAD_FACTOR = 0.75f;

// The initial capacity of the map, in cells.
static const size_t INITIAL_CAPACITY = 16;

// An invidual cell in the internal table.
typedef struct cell
{
    map_key_t key;
    void*     value;
} cell_t;

// Internally, the map is composed of a large, contiguous
// array of cells, organized into pages that are protected
// by a single reader-writer lock.
typedef pthread_rwlock_t page_lock_t;

// A map instance.
struct flat_map
{
    // The global map lock; utilized for resize only.
    // Static after map initialization.
    pthread_rwlock_t map_lock;

    // The internal table; a contiguous array of cells,
    // organzied into disjoint pages each protected by distinct rwlock.
    // Only updated under exclusive map lock.
    cell_t* cells;

    // The array of page locks, one per page in the table.
    page_lock_t* page_locks;

    // The current number of pages that compose page array.
    // Only updated under exclusive map lock.
    size_t n_pages;

    // The number of cells in an individual page.
    // Static after map initialization.
    size_t cells_per_page;

    // The user-provided delete function.
    // Static after map initialization.
    deleter_f deleter;

    // The current count of occupied cells in the map;
    // this count includes tombstone cells, decremented on resize.
    // Updated concurrently by insert operations.
    size_t occupied_cells;
};

static bool is_power_of_two(size_t n);

static size_t get_capacity(flat_map_t* map);

static uint32_t get_hash(map_key_t key);
static size_t get_cell_index_for_hash(flat_map_t* map, uint32_t hash);
static size_t get_page_index_for_hash(flat_map_t* map, uint32_t hash);

static void lock_map_rw(flat_map_t* map);
static void lock_map_resize(flat_map_t* map);
static void unlock_map(flat_map_t* map);

static void lock_page_read(page_lock_t* locks, size_t page_index);
static void lock_page_write(page_lock_t* locks, size_t page_index);
static void unlock_page(page_lock_t* locks, size_t page_index);

static bool insert_at(
    flat_map_t* map,
    size_t      cell_index,
    map_key_t   key,
    void*       value,
    void**      replaced,
    bool*       continue_search);
static bool remove_at(
    flat_map_t* map, 
    size_t      cell_index, 
    uint32_t    key, 
    bool*       continue_search);
static void* find_at(
    flat_map_t* map, 
    size_t      cell_index, 
    uint32_t    key, 
    bool*       continue_search);

static bool need_resize(size_t occupied_cells, size_t capacity);
static void resize_map(flat_map_t* map);

static bool initialize_map_lock(flat_map_t* map);
static void destroy_map_lock(flat_map_t* map);

static cell_t* new_cells(size_t n_cells);
static void initialize_cells(
    cell_t* cells, 
    size_t n_cells);
static void destroy_cells(
    cell_t* cells,
    size_t n_cells,
    deleter_f deleter);

static page_lock_t* new_page_locks(size_t n_locks);
static void initialize_page_locks(
    page_lock_t* page_locks,
    size_t n_locks);
static void destroy_page_locks(
    page_lock_t* page_locks, 
    size_t n_locks);

// ----------------------------------------------------------------------------
// Exported

flat_map_t* flat_map_new(
    size_t    page_size, 
    deleter_f deleter)
{
    if (!is_power_of_two(page_size) 
     || NULL == deleter)
    {
        return NULL;
    }

    flat_map_t* map = malloc(sizeof(flat_map_t));
    if (NULL == map)
    {
        return NULL;
    }

    // initialize the top-level map lock
    if (!initialize_map_lock(map))
    {
        free(map);
        return NULL;
    }

    // compute the initial number of pages we need
    const size_t n_pages = INITIAL_CAPACITY / page_size;

    // allocate the cell array
    cell_t* cells = new_cells(INITIAL_CAPACITY);
    if (NULL == cells)
    {
        pthread_rwlock_destroy(&map->map_lock);
        free(map);
        return NULL;
    }

    // allocate the page locks array
    page_lock_t* page_locks = new_page_locks(n_pages);
    if (NULL == page_locks)
    {
        destroy_cells(cells, INITIAL_CAPACITY, NULL);
        pthread_rwlock_destroy(&map->map_lock);
        free(map);
        return NULL;
    }

    map->cells      = cells;
    map->page_locks = page_locks;

    map->n_pages        = n_pages;
    map->cells_per_page = page_size;

    map->deleter = deleter;

    map->occupied_cells = 0;

    return map;
}

void flat_map_delete(flat_map_t* map)
{
    if (NULL == map)
    {
        return;
    }

    const size_t capacity = map->n_pages*map->cells_per_page;

    destroy_page_locks(map->page_locks, map->n_pages);
    destroy_cells(map->cells, capacity, map->deleter);
    destroy_map_lock(map);
    free(map);
}

bool flat_map_insert(
    flat_map_t* map, 
    map_key_t   key, 
    void*       value, 
    void**      out)
{
    if (NULL == map || 0 == key)
    {
        return false;
    }

    if (out != NULL)
    {
        *out = NULL;
    }

    lock_map_rw(map);

    // compute the current total capacity of the map
    const size_t capacity = map->n_pages*map->cells_per_page;

    // determine if a resize is required
    if (need_resize(map->occupied_cells + 1, capacity))
    {
        // release the shared lock
        unlock_map(map);
        
        // acquire exclusive access to map for resize
        resize_map(map);

        // re-acquire shared lock
        lock_map_rw(map);
    }

    // hash the key
    const uint32_t hash = get_hash(key);

    // locate the appropriate cell index to begin search
    size_t cell_index = get_cell_index_for_hash(map, hash);
    // locate the appropriate page index to begin search
    size_t page_index = get_page_index_for_hash(map, hash);
    
    // track the starting cell index so that if we make a full
    // cycle of the table without finding key, we terminate search
    const size_t init_cell_index = page_index * map->cells_per_page;
    
    bool inserted        = false;
    bool continue_search = true;

    do
    {
        // acquire exclusive access to the page
        lock_page_write(map->page_locks, page_index);

        // search for the key in the current page
        inserted = insert_at(map, cell_index, 
            key, value, out, &continue_search);
        if (!continue_search)
        {
            // key was removed, or empty cell encountered
            break;
        }

        // release exclusive access to the page
        unlock_page(map->page_locks, page_index);

        // increment the page and cell indices for next iteration
        page_index = (page_index + 1) % map->n_pages;
        cell_index = page_index * map->cells_per_page;

    } while (cell_index != init_cell_index);

    unlock_map(map);

    if (inserted)
    {
        // need to atomically increment the number of occupied cells
        // because no lock is held here and this field may be 
        // incremented concurrently by mutliple insert operations
        __atomic_fetch_add(&map->occupied_cells, 1, __ATOMIC_SEQ_CST);
    }

    return inserted;
}

bool flat_map_remove(flat_map_t* map, map_key_t key)
{
    if (NULL == map || 0 == key)
    {
        return false;
    }

    lock_map_rw(map);

    // hash the key
    const uint32_t hash = get_hash(key);

    // locate the appropriate cell index to begin search
    size_t cell_index = get_cell_index_for_hash(map, hash);
    // locate the appropriate page index to begin search
    size_t page_index = get_page_index_for_hash(map, hash);
    
    // track the starting cell index so that if we make a full
    // cycle of the table without finding key, we terminate search
    const size_t init_cell_index = page_index * map->cells_per_page;
    
    bool removed         = false;
    bool continue_search = true;

    do
    {
        // acquire exclusive access to the page
        lock_page_write(map->page_locks, page_index);

        // search for the key in the current page
        removed = remove_at(map, cell_index, key, &continue_search);
        if (!continue_search)
        {
            // key was removed, or empty cell encountered
            break;
        }

        // release exclusive access to the page
        unlock_page(map->page_locks, page_index);

        // increment the page and cell indices for next iteration
        page_index = (page_index + 1) % map->n_pages;
        cell_index = page_index * map->cells_per_page;

    } while (cell_index != init_cell_index);

    unlock_map(map);

    return removed;
}

void* flat_map_find(flat_map_t* map, map_key_t key)
{
    if (NULL == map || 0 == key)
    {
        return NULL;
    }

    lock_map_rw(map);

    // hash the key
    const uint32_t hash = get_hash(key);

    // locate the appropriate cell index to begin search
    size_t cell_index = get_cell_index_for_hash(map, hash);
    // locate the appropriate page index to begin search
    size_t page_index = get_page_index_for_hash(map, hash);
    
    // track the starting cell index so that if we make a full
    // cycle of the table without finding key, we terminate search
    const size_t init_cell_index = page_index * map->cells_per_page;
    
    void* value          = NULL;
    bool continue_search = true;

    do
    {
        // acquire shared access to the page
        lock_page_read(map->page_locks, page_index);

        // search for the key in the current page
        value = find_at(map, cell_index, key, &continue_search);
        if (!continue_search)
        {
            // key was found, or empty cell encountered
            break;
        }

        // release shared access to the page
        unlock_page(map->page_locks, page_index);

        // increment the page and cell indices for next iteration
        page_index = (page_index + 1) % map->n_pages;
        cell_index = page_index * map->cells_per_page;

    } while (cell_index != init_cell_index);

    unlock_map(map);

    return value;
}   

bool flat_map_contains(flat_map_t* map, map_key_t key)
{
    return flat_map_find(map, key) != NULL;
}

// ----------------------------------------------------------------------------
// Internal: Utilities

// determine if the given value is a power of 2.
static bool is_power_of_two(size_t n)
{
    return (n != 0) && ((n & (n - 1)) == 0);
}

// determine the current total capacity of the map
static size_t get_capacity(flat_map_t* map)
{
    return map->n_pages*map->cells_per_page;
}

static uint32_t get_hash(map_key_t key)
{
    unsigned char buffer[sizeof(uint32_t)];
    MurmurHash3_x86_32((void*)&key, sizeof(map_key_t), 0, buffer);
    return (*(uint32_t*)buffer);
}

static size_t get_cell_index_for_hash(flat_map_t* map, uint32_t hash)
{
    const size_t capacity = get_capacity(map);
    return hash & (capacity - 1);
}

static size_t get_page_index_for_hash(flat_map_t* map, uint32_t hash)
{
    // determine the appropriate cell index
    const size_t cell_index = get_cell_index_for_hash(map, hash);

    // compute the corresponding page index
    return cell_index / map->cells_per_page;
}

// acquire shared access to the map for read / write operations
static void lock_map_rw(flat_map_t* map)
{
    pthread_rwlock_rdlock(&map->map_lock);
}

// acquire exclusive access to the map for resize
static void lock_map_resize(flat_map_t* map)
{
    pthread_rwlock_wrlock(&map->map_lock);
}

// release the map lock
static void unlock_map(flat_map_t* map)
{
    pthread_rwlock_unlock(&map->map_lock);
}

static void lock_page_read(page_lock_t* locks, size_t page_index)
{
    page_lock_t* lock = (page_lock_t*) ((unsigned char*)locks + sizeof(page_lock_t)*page_index);
    pthread_rwlock_rdlock(lock);
}

static void lock_page_write(page_lock_t* locks, size_t page_index)
{
    page_lock_t* lock = (page_lock_t*) ((unsigned char*)locks + sizeof(page_lock_t)*page_index);
    pthread_rwlock_wrlock(lock);
}

static void unlock_page(page_lock_t* locks, size_t page_index)
{
    page_lock_t* lock = (page_lock_t*) ((unsigned char*)locks + sizeof(page_lock_t)*page_index);
    pthread_rwlock_unlock(lock);
}

// ----------------------------------------------------------------------------
// Internal: Insert, Remove, Find

static bool insert_at(
    flat_map_t* map,
    size_t      cell_index,
    map_key_t   key,
    void*       value,
    void**      replaced,
    bool*       continue_search)
{
    *continue_search = true;
    bool inserted    = false;

    // compute the index of the cell relative to page
    const size_t index_in_page = cell_index % map->cells_per_page;
    // compute the index at which iteration for this page should end (exclusive)
    const size_t end_index = cell_index + (map->cells_per_page - index_in_page);

    // iterate over all cells in the page
    for (size_t i = cell_index; i < end_index; ++i)
    {
        // locate a single cell
        cell_t* cell = &map->cells[i];

        if (cell->key == key)
        {
            // found a matching key, replace the value
            if (replaced != NULL)
            {
                *replaced = cell->value;
            }

            cell->value       = value;
            inserted         = true;
            *continue_search = false;
            break;
        }

        if (cell->key == EMPTY_KEY)
        {
            // found an empty cell, insert here
            cell->key        = key;
            cell->value      = value;
            inserted         = true;
            *continue_search = false;
            break;
        }
    }

    // if we fall off the end of the page without finding 
    // a matching key or an empty cell, we must continue
    // the search on the following page

    return inserted;  
}

static bool remove_at(
    flat_map_t* map, 
    size_t      cell_index, 
    uint32_t    key, 
    bool*       continue_search)
{
    *continue_search = true;
    bool removed     = false;

    // compute the index of the cell relative to page
    const size_t index_in_page = cell_index % map->cells_per_page;
    // compute the index at which iteration for this page should end (exclusive)
    const size_t end_index = cell_index + (map->cells_per_page - index_in_page);

    // iterate over all cells in the page
    for (size_t i = cell_index; i < end_index; ++i)
    {
        // locate a single cell
        cell_t* cell = &map->cells[i];

        if (cell->key == key)
        {
            // found a match
            
            // delete the stored value
            map->deleter(cell->value);
            
            // mark the cell with a tombstone
            cell->key = TOMBSTONE_KEY;

            removed          = true;
            *continue_search = false;
            break;
        }

        if (cell->key == EMPTY_KEY)
        {
            // found an empty cell, search is over
            removed          = false;
            *continue_search = false;
            break;
        }
    }

    // if we fall off the end of the page without finding 
    // a matching key or an empty cell, we must continue
    // the search on the following page

    return removed;
}

static void* find_at(
    flat_map_t* map, 
    size_t      cell_index, 
    uint32_t    key, 
    bool*       continue_search)
{
    *continue_search = true;
    void* value      = NULL;

    // compute the index of the cell relative to page
    const size_t index_in_page = cell_index % map->cells_per_page;
    // compute the index at which iteration for this page should end (exclusive)
    const size_t end_index = cell_index + (map->cells_per_page - index_in_page);

    // iterate over all cells in the page
    for (size_t i = cell_index; i < end_index; ++i)
    {
        // locate a single cell
        cell_t* cell = &map->cells[i];

        if (cell->key == key)
        {
            // found a match for our query
            value = cell->value;
            *continue_search = false;
            break;
        }

        if (cell->key == EMPTY_KEY)
        {
            // found an empty cell, terminate search
            *continue_search = false;
            break;
        }
    }

    // if we fall off the end of the page without finding 
    // a matching key or an empty cell, we must continue
    // the search on the following page

    return value;
}

// ----------------------------------------------------------------------------
// Internal: Map Resize

static bool need_resize(size_t occupied_cells, size_t capacity)
{
    return occupied_cells >= capacity*LOAD_FACTOR;
}

static void resize_map(flat_map_t* map)
{
    lock_map_resize(map);

    const size_t capacity = map->n_pages*map->cells_per_page;
    if (!need_resize(map->occupied_cells, capacity))
    {
        // race for resize operation occurred, and we lost
        unlock_map(map);
        return;
    }

    // we now have exclusive access to the entire map structure

    // hang on to old references
    const size_t old_n_pages  = map->n_pages;
    const size_t old_capacity = old_n_pages*map->cells_per_page;

    cell_t* old_cells      = map->cells;
    page_lock_t* old_locks = map->page_locks;

    // double the map capacity
    const size_t new_n_pages  = map->n_pages << 1;
    const size_t new_capacity = new_n_pages*map->cells_per_page;

    map->cells          = new_cells(new_capacity);
    map->page_locks     = new_page_locks(new_n_pages);
    map->n_pages        = new_n_pages;
    map->occupied_cells = 0;

    // iterate over each existing cell in the map
    for (size_t i = 0; i < old_capacity; ++i)
    {
        cell_t cell = old_cells[i];

        if (cell.key == EMPTY_KEY || cell.key == TOMBSTONE_KEY)
        {
            continue;
        }

        // compute the new cell index for the cell
        const uint32_t hash = get_hash(cell.key);

        uint32_t cell_index = get_cell_index_for_hash(map, hash);
        uint32_t page_index = get_page_index_for_hash(map, hash);

        bool continue_search = true;
        do
        {
            insert_at(map, cell_index, 
                cell.key, cell.value, NULL, &continue_search);
            
            page_index = (page_index + 1) % new_n_pages;
            cell_index = page_index * map->cells_per_page;
        }
        while (continue_search);
    }

    unlock_map(map);

    // cleanup the old structures
    destroy_cells(old_cells, old_capacity, NULL);
    destroy_page_locks(old_locks, old_n_pages);
}

// ----------------------------------------------------------------------------
// Internal: Component Initialization and Destruction

// initialize the top-level map lock
static bool initialize_map_lock(flat_map_t* map)
{
    // set the atributes of the global rwlock such 
    // that writer starvation is prevented
    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
    pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);

    return pthread_rwlock_init(&map->map_lock, &attr) == 0;
}

static void destroy_map_lock(flat_map_t* map)
{
    pthread_rwlock_destroy(&map->map_lock);
}

static cell_t* new_cells(size_t n_cells)
{
    cell_t* cells = calloc(n_cells, sizeof(cell_t));
    if (NULL == cells)
    {
        return NULL;
    }

    initialize_cells(cells, n_cells);

    return cells;
}

// initialize each cell in the table
static void initialize_cells(
    cell_t* cells, 
    size_t n_cells)
{
    for (size_t i = 0; i < n_cells; ++i)
    {
        cell_t* cell = &cells[i];
        cell->key    = EMPTY_KEY;
        cell->value  = NULL;
    }
}

// deallocate the data stored in each cell
static void destroy_cells(
    cell_t* cells,
    size_t n_cells,
    deleter_f deleter)
{
    if (deleter != NULL)
    {
        for (size_t i = 0; i < n_cells; ++i)
        {
            cell_t cell = cells[i];
            deleter(cell.value);
        }
    }

    free(cells);
}


static page_lock_t* new_page_locks(size_t n_locks)
{
    page_lock_t* page_locks = calloc(n_locks, sizeof(page_lock_t));
    if (NULL == page_locks)
    {
        return NULL;
    }

    initialize_page_locks(page_locks, n_locks);

    return page_locks;
}

// initialize the page lock for each page
static void initialize_page_locks(
    page_lock_t* page_locks,
    size_t n_locks)
{
    for (size_t i = 0; i < n_locks; ++i)
    {
        page_lock_t lock = page_locks[i];
        pthread_rwlock_init(&lock, NULL);
    }
}

// destroy the per-page lock for each page
static void destroy_page_locks(
    page_lock_t* page_locks, 
    size_t n_locks)
{
    for (size_t i = 0; i < n_locks; ++i)
    {
        page_lock_t lock = page_locks[i];
        pthread_rwlock_destroy(&lock);
    }
}