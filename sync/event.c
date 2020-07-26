// event.h
// A simple inter-thread synchronization primitive built 
// on top of existing pthreads primitives.

#include "event.h"

#define _GNU_SOURCE
#include <stdlib.h>
#include <pthread.h>

// ----------------------------------------------------------------------------
// Exported

bool event_init(event_t* event)
{
    if (NULL == event)
    {
        return false;
    }

    const int mu_init = pthread_mutex_init(&event->mu, NULL);
    const int cv_init = pthread_cond_init(&event->cv, NULL);

    return 0 == mu_init && 0 == cv_init;
}

void event_destroy(event_t* event)
{
    if (NULL == event)
    {
        return;
    }

    pthread_mutex_destroy(&event->mu);
    pthread_cond_destroy(&event->cv);
}

void event_wait(event_t* event)
{
    if (NULL == event)
    {
        return;
    }

    pthread_mutex_lock(&event->mu);
    pthread_cond_wait(&event->cv, &event->mu);
    pthread_mutex_unlock(&event->mu);
}

void event_post(event_t* event)
{
    if (NULL == event)
    {
        return;
    }

    pthread_cond_signal(&event->cv);
}

void event_broadcast(event_t* event)
{
    if (NULL == event)
    {
        return;
    }

    pthread_cond_broadcast(&event->cv);
}