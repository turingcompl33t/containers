// gc.h
// A garbage collector instance for managing RCU.

#ifndef GC_H
#define GC_H

#include <stddef.h>

typedef struct gc gc_t;

// The signature for the deferred deletion function.
typedef void (*deleter_f)(void*);

// gc_new()
gc_t* gc_new(void);

// gc_delete()
void gc_delete(gc_t* gc);

// gc_get_generation()
size_t gc_get_generation(gc_t* gc);

// gc_inc_generation()
size_t gc_inc_generation(gc_t* gc);

// gc_inc_rc()
void gc_inc_rc(gc_t* gc, size_t generation);

// gc_dec_rc()
void gc_dec_rc(gc_t* gc, size_t generation);

// gc_rc_for_generation();
size_t gc_rc_for_generation(gc_t* gc, size_t generation);

// gc_defer_destroy()
void gc_defer_destroy(gc_t* gc, deleter_f deleter, void* object);

// gc_collect_through_generation()
void gc_collect_through_generation(gc_t* gc, size_t generation);

#endif // GC_H