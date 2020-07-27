// test.c

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "rcu_list.h"

typedef struct dummy
{
    int a;
    int b;
} dummy_t;

static dummy_t* make_dummy(int a, int b)
{
    dummy_t* dummy = malloc(sizeof(dummy_t));
    dummy->a = a;
    dummy->b = b;
    return dummy;
}

// static void destroy_dummy(dummy_t* d)
// {
//     free(d);
// }

int main(void)
{
    rcu_list_t* list = list_new();

    dummy_t* d1 = make_dummy(1, 1);
    dummy_t* d2 = make_dummy(2, 2);
    
    write_handle_t whandle = list_register_writer(list);
    read_handle_t rhandle = list_register_reader(list);
    
    rcu_write_lock(&whandle);

    // rwite to list here
    list_push_front(list, d1, &whandle);
    list_push_front(list, d2, &whandle);

    rcu_write_unlock(&whandle);

    rcu_read_lock(&rhandle);

    // read from list here
    iterator_t first = list_begin(list, &rhandle);
    void* data = iterator_get(&first);

    assert(data != NULL);

    dummy_t* as_d2 = (dummy_t*) data;
    assert(as_d2->a == 2);
    assert(as_d2->b == 2);

    rcu_read_unlock(&rhandle);

    list_delete(list);

    return EXIT_SUCCESS;
}