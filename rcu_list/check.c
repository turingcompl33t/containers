// check.c
// Driver program for RCU list data structure tests.

// attribute gnu_printf
#pragma GCC diagnostic ignored "-Wignored-attributes"

#include <check.h>
#include <stdlib.h>

#include "rcu_list.h"

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

START_TEST(test_rcu_list_new)
{
    rcu_list_t* list = list_new();
    ck_assert(list != NULL);

    list_delete(list);
}
END_TEST

// ----------------------------------------------------------------------------
// Infrastructure
 
Suite* rcu_list_suite(void)
{
    Suite* s = suite_create("rcu-list");
    TCase* tc_core = tcase_create("rcu-list-core");
    
    tcase_add_test(tc_core, test_rcu_list_new);

    suite_add_tcase(s, tc_core);
    
    return s;
}

int main(void)
{
    Suite* suite = rcu_list_suite();
    SRunner* runner = srunner_create(suite);

    srunner_run_all(runner, CK_NORMAL);    
    srunner_free(runner);
    
    return EXIT_SUCCESS;
}