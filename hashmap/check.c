// check.c
// Driver program for chaining hashmap data structure tests.

// attribute gnu_printf
#pragma GCC diagnostic ignored "-Wignored-attributes"

#include <check.h>
#include <stdlib.h>

#include "hashmap.h"

// ----------------------------------------------------------------------------
// Definitions for Testing

typedef struct point
{
    float x;
    float y;
} point_t;

// static point_t* make_point(float x, float y)
// {
//     point_t* p = malloc(sizeof(point_t));
//     p->x = x;
//     p->y = y;

//     return p;
// }

// static void delete_point(void* p)
// {
//     point_t* as_point = (point_t*)p;
//     free(as_point);
// }

// ----------------------------------------------------------------------------
// Test Cases

START_TEST(test_hashmap_new)
{
    hashmap_t* map = hashmap_new();
    ck_assert(map != NULL);

    hashmap_delete(map);
}
END_TEST

// ----------------------------------------------------------------------------
// Infrastructure
 
Suite* hashmap_suite(void)
{
    Suite* s = suite_create("hashmap");
    TCase* tc_core = tcase_create("hashmap-core");
    
    tcase_add_test(tc_core, test_hashmap_new);

    suite_add_tcase(s, tc_core);
    
    return s;
}

int main(void)
{
    Suite* suite = hashmap_suite();
    SRunner* runner = srunner_create(suite);

    srunner_run_all(runner, CK_NORMAL);    
    srunner_free(runner);
    
    return EXIT_SUCCESS;
}