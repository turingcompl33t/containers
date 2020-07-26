// rcu.h
// Barebones RCU memory reclamation.

#ifndef RCU_H
#define RCU_H

#include "gc.h"

typedef struct rcu_handle rcu_handle_t;

// ----------------------------------------------------------------------------
// Exported: Reader Interface

rcu_handle_t rcu_enter(gc_t* gc);

void rcu_leave(gc_t* gc, rcu_handle_t handle);

// ----------------------------------------------------------------------------
// Exported: Writer Interface

void rcu_defer(gc_t* gc, deleter_f deleter, void* object);

void rcu_synchronize(gc_t* gc);

#endif // RCU_H