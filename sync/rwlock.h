// rwlock.h
// A write-preferring reader-writer lock.
//
// This implementation is adapted from the that utilized 
// by the sync.RWMutex type from the Go standard libary.

#ifndef RWLOCK_H
#define RWLOCK_H

#include "event.h"

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>

typedef struct rwlock
{
    pthread_mutex_t mutex;

    event_t reader_release;
    event_t writer_release;

    int_fast32_t n_pending;
    int_fast32_t readers_departing;
} rwlock_t;

// rwlock_init()
//
// Initialize a new lock.
bool rwlock_init(rwlock_t* lock);

// rwlock_destroy()
//
// Destroy an existing lock.
void rwlock_destroy(rwlock_t* lock);

// rwlock_lock_read()
//
// Acquire the lock with shared access.
void rwlock_lock_read(rwlock_t* lock);

// rwlock_unlock_read()
//
// Release shared access to the lock.
void rwlock_unlock_read(rwlock_t* lock);

// rwlock_lock_write()
//
// Acquire the lock with exclusive access.
void rwlock_lock_write(rwlock_t* lock);

// rwlock_unlock_write()
//
// Release exclusive access to the lock.
void rwlock_unlock_write(rwlock_t* lock);

#endif // RWLOCK_H