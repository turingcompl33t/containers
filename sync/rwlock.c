// rwlock.c
// A write-preferring reader-writer lock.
//
// This implementation is adapted from the that utilized 
// by the sync.RWMutex type from the Go standard libary.

#include "event.h"
#include "rwlock.h"

#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

// The maxmimum number of concurrent readers.
static const int_fast32_t MAX_READERS = 1 << 30;

bool rwlock_init(rwlock_t* lock)
{
    if (NULL == lock)
    {
        return false;
    }

    const int mu_init = pthread_mutex_init(&lock->mutex, NULL);
    const bool rr_init = event_init(&lock->reader_release);
    const bool wr_init = event_init(&lock->writer_release);

    if (mu_init != 0 || !rr_init || !wr_init)
    {
        return false;
    }

    lock->n_pending         = 0;
    lock->readers_departing = 0;

    return true;
}

void rwlock_destroy(rwlock_t* lock)
{
    if (NULL == lock)
    {
        return;
    }

    pthread_mutex_destroy(&lock->mutex);
    event_destroy(&lock->reader_release);
    event_destroy(&lock->writer_release);
}

void rwlock_lock_read(rwlock_t* lock)
{
    if (NULL == lock)
    {
        return;
    }

    // The reader atomically adds 1 to n_pending; if the resulting
    // value is nonnegative, their are no pending writers and the 
    // reader may proceed. Thus, on the expected common path, no 
    // locks need be acquired. Otherwise, their is a writer either 
    // using the lock or waiting for readers to exit so that it may
    // acquire it. In this case, the reader yields to the writer. 

    if (__atomic_add_fetch(&lock->n_pending, 1, __ATOMIC_SEQ_CST) < 0)
    {
        event_wait(&lock->reader_release);
    }
}

void rwlock_unlock_read(rwlock_t* lock)
{
    if (NULL == lock)
    {
        return;
    }

    // When a reader completes, it decrements n_pending. If there
    // are no writers pending (determined by a negative value for
    // n_pending) then the reader is done. Again, on this path, no
    // interaction with locks is required. Otherwise, if a writer 
    // is waiting to acquire the lock, and if this is the last 
    // reader to exit the lock since the writer began waiting, the
    // reader wakes the writer to notify them they may now proceed.

    if (__atomic_sub_fetch(&lock->n_pending, 1, __ATOMIC_SEQ_CST) < 0)
    {
        if (__atomic_sub_fetch(&lock->readers_departing, 1, __ATOMIC_SEQ_CST) == 0)
        {
            event_post(&lock->reader_release);
        }
    }
}

void rwlock_lock_write(rwlock_t* lock)
{
    if (NULL == lock)
    {
        return;
    }

    // The embedded mutex is used to ensure that only a single writer
    // is ever active in this critical section at any one time. The 
    // computation on the following line performs two operations:
    //
    // - Notify readers that a writer is pending by subtracting MAX_READERS from n_pending
    // - Computes the total number of current readers by adding MAX_READERS back to the result
    //
    // Next, if there are any active readers (r > 0), the writer sets this 
    // value into readers_departing such that each reader that subsequently leaves
    // the lock may know when / if it is the last reader to do so such that
    // it may wake up the pending writer.

    pthread_mutex_lock(&lock->mutex);

    const int_fast32_t r 
        = __atomic_sub_fetch(&lock->n_pending, MAX_READERS, __ATOMIC_SEQ_CST) + MAX_READERS;

    if (r != 0 && __atomic_add_fetch(&lock->readers_departing, r, __ATOMIC_SEQ_CST) != 0)
    {
        event_wait(&lock->writer_release);
    }
}

void rwlock_unlock_write(rwlock_t* lock)
{
    if (NULL == lock)
    {
        return;
    }

    // When releasing the exclusive lock, the writer adds MAX_READERS
    // back into n_pending, informing readers that there are no more
    // writers using or pending on the lock. The remaining value in
    // n_pending (positive) is the number of readers that accumulated
    // while the writer held the lock, but it is not necessary to utilize
    // this information to wake all waiting readers because we have
    // access to broadcast functionality with event_broadcast(). Finally,
    // the writer releases the embedded mutex so that future writers may
    // proceed and annouce their intention to begin the process anew.

    __atomic_add_fetch(&lock->n_pending, MAX_READERS, __ATOMIC_SEQ_CST);

    event_broadcast(&lock->reader_release);

    pthread_mutex_unlock(&lock->mutex);
}