// priority_queue.h
// Single-threaded priority queue.

#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <stdbool.h>

typedef bool (*prioritizer_f)(void*, void*);

typedef bool (*predicate_f)(void*, void*);

typedef struct queue queue_t;

queue_t* queue_new(prioritizer_f prioritizer);

void queue_delete(queue_t* queue);

bool queue_push(queue_t* queue, void* data);

void* queue_pop(queue_t* queue);

void* queue_pop_if(queue_t* queue, predicate_f pred, void* ctx);

#endif // PRIORITY_QUEUE_H