// rwlock_driver.c

#include "rwlock.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define N_ITEMS 1000
#define N_ITERS 1

#define N_READERS 10
#define N_WRITERS  3

typedef struct arg
{
    rwlock_t* lock;
    int*      data;
} arg_t;

static void init_data(int* data, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        data[i] = i;
    }
}

static void* reader(void* arg)
{
    arg_t* a = (arg_t*) arg;
    
    int* data = a->data;
    rwlock_t* lock = a->lock;

    for (size_t iters = 0; iters < N_ITERS; ++iters)
    {
        rwlock_lock_read(lock);

        for (size_t i = 1; i < N_ITEMS; ++i)
        {
            const bool r = (data[i] == data[i-1]+1);
            assert(r);
        }

        rwlock_unlock_read(lock);
    }

    return EXIT_SUCCESS;
}

static void* writer(void* arg)
{
    arg_t* a = (arg_t*) arg;
    
    int* data = a->data;
    rwlock_t* lock = a->lock;

    for (size_t iter = 0; iter < N_ITERS; ++iter)
    {
        // TODO: time taken to acquire lock

        rwlock_lock_write(lock);

        for (size_t i = 0; i < N_ITEMS; ++i)
        {
            data[i] += 1;
        }

        rwlock_unlock_write(lock);
    }

    return EXIT_SUCCESS;
}

int main(void)
{
    rwlock_t lock;

    pthread_t readers[N_READERS];
    pthread_t writers[N_WRITERS];

    if (!rwlock_init(&lock))
    {
        printf("[-] rwlock_init() failed\n");
        return EXIT_FAILURE;
    }

    int* data = calloc(N_ITEMS, sizeof(int));
    if (NULL == data)
    {
        printf("[-] allocation failure\n");
        return EXIT_FAILURE;
    }

    // initialize data array with monotonically increasing values
    init_data(data, N_ITEMS);

    arg_t arg = {
        .data = data,
        .lock = &lock
    };

    // launch many concurrent readers
    for (size_t i = 0; i < N_READERS; ++i)
    {
        pthread_create(&readers[i], NULL, reader, &arg);
    }

    // launch some concurrent writers
    for (size_t i = 0; i < N_WRITERS; ++i)
    {
        pthread_create(&writers[i], NULL, writer, &arg);
    }

    // wait for everyone to complete

    void* ret;
    for (size_t i = 0; i < N_WRITERS; ++i)
    {
        pthread_join(writers[i], &ret);
    }
    for (size_t i = 0; i < N_READERS; ++i)
    {
        pthread_join(readers[i], &ret);
    }

    // cleanup

    free(data);
    rwlock_destroy(&lock);

    printf("[+] success\n");

    return EXIT_SUCCESS;
}