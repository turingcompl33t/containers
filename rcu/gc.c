// gc.c
// A garbage collector instance for managing RCU.

#define _GNU_SOURCE

#include "gc.h"
#include "priority_queue.h"
#include "intrusive_list.h"
#include "../sync/event.h"

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
    // The last generation for which garbage has been collected.
    size_t last_gc_gen;

    // The head of the intrusive list of reference counts.
    list_head_t ref_counts;
    // Priority queue of deferred functions.
    queue_t* deferred;

    // The event used to wake writers on generation end.
    event_t generation_complete;
};

// A generation reference count tracker.
typedef struct ref_count
{
    list_entry_t entry;
    size_t       generation;
    size_t       count;
} ref_count_t;

// A single entry in the list of deferred functions.
typedef struct deferred
{
    deleter_f    deleter;    // the deferred function 
    void*        object;     // the object to be destroyed
    size_t       generation; // the generation in which garbage was created
} deferred_t;

static ref_count_t* make_ref_count(size_t generation);
static void destroy_ref_count(ref_count_t* rc);

static deferred_t* make_deferred(
    deleter_f deleter, 
    void*     object, 
    size_t    generation);
static void destroy_deferred(deferred_t* deferred);

static void initialize_list_head(list_head_t* list);

static void lock_list_read(list_head_t* list);
static void lock_list_write(list_head_t* list);
static void unlock_list(list_head_t* list);

static bool find_rc_by_generation(list_entry_t* entry, void* ctx);

static bool prioritize_by_generation(void* d1, void* d2);
static bool generation_is(void* deferred, void* ctx);

static size_t atomic_load_n(size_t* n);
static size_t atomic_increment(size_t* n);
static size_t atomic_decrement(size_t* n);

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
    event_init(&gc->generation_complete);

    gc->deferred = queue_new(prioritize_by_generation);

    gc->current_generation = 0;
    gc->last_gc_gen        = 0;

    // initialize the gc with the first generation
    ref_count_t* rc = make_ref_count(0);
    if (NULL == rc)
    {
        free(gc);
        return NULL;
    }

    list_push_back(&gc->ref_counts.head, &rc->entry);

    return gc;
}

void gc_delete(gc_t* gc)
{
    if (NULL == gc)
    {
        return;
    }

    queue_delete(gc->deferred);
    event_destroy(&gc->generation_complete);
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
        &gc->ref_counts.head, find_rc_by_generation, (void*) generation);
    assert(rc != NULL);

    atomic_increment(&rc->count);

    unlock_list(&gc->ref_counts);
}

void gc_dec_rc(gc_t* gc, size_t generation)
{
    lock_list_read(&gc->ref_counts);

    ref_count_t* rc = (ref_count_t*) list_find(
        &gc->ref_counts.head, find_rc_by_generation, (void*) generation);
    assert(rc != NULL);

    const size_t count = atomic_decrement(&rc->count);
    if (0 == count)
    {
        // inform a waiting writer in rcu_synchronize()
        // that generation is available for collection
        event_post(&gc->generation_complete);
    }

    unlock_list(&gc->ref_counts);
}

size_t gc_rc_for_generation(gc_t* gc, size_t generation)
{
    lock_list_read(&gc->ref_counts);

    ref_count_t* rc = (ref_count_t*) list_find(
        &gc->ref_counts.head, find_rc_by_generation, (void*) generation);
    assert(rc != NULL);

    const size_t count = atomic_load_n(&rc->count);

    unlock_list(&gc->ref_counts);

    return count;
}

void gc_defer_destroy(gc_t* gc, deleter_f deleter, void* object)
{
    deferred_t* deferred = make_deferred(deleter, object, gc_get_generation(gc));
    if (NULL == deferred)
    {
        return;
    }

    // add the deferred function to global queue
    queue_push(gc->deferred, deferred);
}

void gc_collect_through_generation(gc_t* gc, size_t generation)
{
    while (gc->last_gc_gen < generation)
    {
        // wait for all outstanding references to drop
        while (gc_rc_for_generation(gc, gc->last_gc_gen) > 0)
        {
            event_wait(&gc->generation_complete);
        }

        // the generation last_gc_gen is now complete, collect garbage

        // continue to remove garbage from global queue until the generation
        // of the deferred function exceeds the current GC generation
        deferred_t* deferred;
        while ((deferred = (deferred_t*) queue_pop_if(
            gc->deferred, generation_is, (void*)gc->last_gc_gen)) != NULL)
        {
            // invoke the deferred function
            deferred->deleter(deferred->object);
            // destroy the deferred instance
            destroy_deferred(deferred);
        }

        // the generation is now collected and will no longer be used,
        // safe to unlink its reference count entry from list
        lock_list_write(&gc->ref_counts);

        // find the refcount for the GCed generation
        ref_count_t* rc = (ref_count_t*) list_find(
            &gc->ref_counts.head, find_rc_by_generation, (void*)gc->last_gc_gen);

        // unlink it from the list and destroy it
        list_remove_entry(&gc->ref_counts.head, &rc->entry);
        destroy_ref_count(rc);

        unlock_list(&gc->ref_counts);

        gc->last_gc_gen++;
    }
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

static deferred_t* make_deferred(
    deleter_f deleter, 
    void*     object, 
    size_t    generation)
{
    deferred_t* deferred = malloc(sizeof(deferred_t));
    if (NULL == deferred)
    {
        return NULL;
    }

    deferred->deleter    = deleter;
    deferred->object     = object;
    deferred->generation = generation;

    return deferred;
}

static void destroy_deferred(deferred_t* deferred)
{
    free(deferred);
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

static bool generation_is(void* deferred, void* ctx)
{
    deferred_t* as_deferred = (deferred_t*) deferred;
    size_t as_generation    = (size_t) ctx;

    return as_deferred->generation == as_generation;
}

static size_t atomic_load_n(size_t* n)
{
    return __atomic_load_n(n, __ATOMIC_ACQUIRE);
}

static size_t atomic_increment(size_t* n)
{
    return __atomic_add_fetch(n, 1, __ATOMIC_RELEASE);
}

static size_t atomic_decrement(size_t* n)
{
    return __atomic_sub_fetch(n, 1, __ATOMIC_RELEASE);
}