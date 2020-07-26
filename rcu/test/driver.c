// driver.c
// Driver program for RCU system.

// attribute gnu_printf
#pragma GCC diagnostic ignored "-Wignored-attributes"

#include <stdlib.h>

#include "../rcu.h"

int main(void)
{
    // construct a new GC instance
    gc_t* gc = gc_new();
    
    
    // destroy the GC instance
    gc_delete(gc);

    return EXIT_SUCCESS;
}
