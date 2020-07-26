// rcu.c
// Barebones RCU memory reclamation.

#include "rcu.h"

#include <stdlib.h>

struct rcu_handle
{
    // The generation in which this handle resides.
    size_t generation;
};

// ----------------------------------------------------------------------------
// Exported: Writer Interface

rcu_handle_t rcu_enter(gc_t* gc)
{
    const size_t gen = gc_get_generation(gc);

    rcu_handle_t handle = {
        .generation = gen
    };

    gc_inc_rc(gc, gen);
    return handle;
}

void rcu_leave(gc_t* gc, rcu_handle_t handle)
{
    const size_t gen = handle.generation;
    gc_dec_rc(gc, gen);
}

// ----------------------------------------------------------------------------
// Exported: Writer Interface

void rcu_defer(gc_t* gc, deleter_f deleter, void* object)
{
    gc_defer_destroy(gc, deleter, object);
}

void rcu_synchronize(gc_t* gc)
{
    const size_t prev_gen = gc_inc_generation(gc);
    gc_collect_through_generation(gc, prev_gen);
}