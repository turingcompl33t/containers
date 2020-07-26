// test.c

#include <stdio.h>
#include <stdlib.h>

#include "rcu_list.h"

int main(void)
{
    rcu_list_t* list = list_new();
    
    read_handle_t rhandle = list_register_reader(list);

    rcu_read_lock(&rhandle);

    // do stuff

    rcu_read_unlock(&rhandle);

    list_delete(list);

    return EXIT_SUCCESS;
}