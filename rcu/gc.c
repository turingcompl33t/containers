// gc.c
// A garbage collector instance for managing RCU.

#define _GNU_SOURCE
#include "gc.h"
#include "priority_queue.h"
#include "intrusive_list.h"

#include <assert.h>
#include <stdlib.h>
#include <pthread.h>

// ----------------------------------------------------------------------------
// Internal Declarations

typedef struct list_head
{
    list_entry_t     head;
    pthread_rwlock_t lock;
} list_head_t;

struct gc
{
    // The current global generation.
    size_t current_generation;
    // The head of the intrusive list of reference counts.
    list_head_t ref_counts;
    // Priority queue of deferred functions.
    queue_t* deferred;
};

// A generation reference count tracker.
typedef struct ref_count
{
    list_entry_t entry;
    size_t       generation;
    size_t       count;
} ref_count_t;

// The signature for the deferred deletion function.
typedef void (*deleter_f)(void*, void*);

// A single entry in the list of deferred functions.
typedef struct deferred
{
    deleter_f    deleter;
    void*        ctx;
    size_t       generation;
} deferred_t;

static ref_count_t* make_ref_count(size_t generation);
static void destroy_ref_count(ref_count_t* rc);

static void initialize_list_head(list_head_t* list);

static void lock_list_read(list_head_t* list);
static void lock_list_write(list_head_t* list);
static void unlock_list(list_head_t* list);

static bool prioritize_by_generation(void* d1, void* d2);

static bool find_rc_by_generation(list_entry_t* entry, void* ctx);

static void atomic_increment(size_t* n);
static void atomic_decrement(size_t* n);

// ----------------------------------------------------------------------------
// Exported

gc_t* gc_new(void)
{
    gc_t* gc = malloc(sizeof(gc_t));
    if (NULL == gc)
    {
        return NULL;
    }

    initialize_list_head(&gc->ref_counts);
    gc->deferred = queue_new(prioritize_by_generation);

    gc->current_generation = 0;

    ref_count_t* rc = make_ref_count(0);
    if (NULL == rc)
    {
        free(gc);
        return NULL;
    }

    // initialize the gc with the first generation
    list_push_back(&gc->ref_counts.head, &rc->entry);
}

void gc_delete(gc_t* gc)
{
    if (NULL == gc)
    {
        return;
    }

    queue_delete(gc->deferred);
    free(gc);
}

size_t gc_get_generation(gc_t* gc)
{
    return __atomic_load_n(
        &gc->current_generation, __ATOMIC_ACQUIRE);
}

size_t gc_inc_generation(gc_t* gc)
{
    return __atomic_fetch_add(
        &gc->current_generation, 1, __ATOMIC_RELEASE);
}

void gc_inc_rc(gc_t* gc, size_t generation)
{
    lock_list_read(&gc->ref_counts);

    ref_count_t* rc = (ref_count_t*) list_find(
        &gc->ref_counts, find_rc_by_generation, (void*) generation);
    assert(rc != NULL);

    unlock_list(&gc->ref_counts);

    atomic_increment(&rc->count);
}

void gc_dec_rc(gc_t* gc, size_t generation)
{
    lock_list_read(&gc->ref_counts);

    ref_count_t* rc = (ref_count_t*) list_find(
        &gc->ref_counts, find_rc_by_generation, (void*) generation);
    assert(rc != NULL);

    unlock_list(&gc->ref_counts);

    atomic_decrement(&rc->count);
}

// ----------------------------------------------------------------------------
// Internal

static ref_count_t* make_ref_count(size_t generation)
{
    ref_count_t* rc = malloc(sizeof(ref_count_t));
    if (NULL == rc)
    {
        return NULL;
    }

    rc->count      = 0;
    rc->generation = generation;

    return rc;
}

static void destroy_ref_count(ref_count_t* rc)
{
    free(rc);
}

static void initialize_list_head(list_head_t* list)
{
    list_init(&list->head);
    pthread_rwlock_init(&list->lock, NULL);
}

static void lock_list_read(list_head_t* list)
{
    pthread_rwlock_rdlock(&list->lock);
}

static void lock_list_write(list_head_t* list)
{
    pthread_rwlock_wrlock(&list->lock);
}

static void unlock_list(list_head_t* list)
{
    pthread_rwlock_unlock(&list->lock);
}

static bool find_rc_by_generation(list_entry_t* entry, void* ctx)
{
    ref_count_t* as_rc   = (ref_count_t*)entry;
    size_t as_generation = (size_t) ctx;

    return as_rc->generation == as_generation;
}

static bool prioritize_by_generation(void* d1, void* d2)
{
    deferred_t* as_d1 = (deferred_t*)d1;
    deferred_t* as_d2 = (deferred_t*)d2;

    return as_d1->generation <= as_d2->generation;
}

static void atomic_increment(size_t* n)
{
    __atomic_fetch_add(n, 1, __ATOMIC_RELEASE);
}

static void atomic_decrement(size_t* n)
{
    __atomic_fetch_sub(n, 1, __ATOMIC_RELEASE);
}