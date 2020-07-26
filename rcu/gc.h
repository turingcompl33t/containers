// gc.h
// A garbage collector instance for managing RCU.

#ifndef GC_H
#define GC_H

typedef struct gc gc_t;

// gc_new()
gc_t* gc_new(void);

// gc_delete()
void gc_delete(gc_t* gc);

size_t gc_get_generation(gc_t* gc);

size_t gc_inc_generation(gc_t* gc);

void gc_inc_rc(gc_t* gc, size_t generation);

void gc_dec_rc(gc_t* gc, size_t generation);

#endif // GC_H