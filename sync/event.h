// event.h
// A simple inter-thread synchronization primitive built 
// on top of existing pthreads primitives.

#ifndef EVENT_H
#define EVENT_H

#include <pthread.h>
#include <stdbool.h>

typedef struct event
{
    pthread_mutex_t mu;
    pthread_cond_t  cv;
} event_t;

bool event_init(event_t* event);

void event_destroy(event_t* event);

void event_wait(event_t* event);

void event_post(event_t* event);

void event_broadcast(event_t* event);

#endif // EVENT_H