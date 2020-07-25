// check.c
// Driver program for flat hashmap data structure tests.

// attribute gnu_printf
#pragma GCC diagnostic ignored "-Wignored-attributes"

#include <check.h>
#include <stdlib.h>

#include "flat_map.h"

// ----------------------------------------------------------------------------
// Definitions for Testing

typedef struct point
{
    float x;
    float y;
} point_t;

static point_t* make_point(float x, float y)
{
    point_t* p = malloc(sizeof(point_t));
    p->x = x;
    p->y = y;

    return p;
}

static void delete_point(void* p)
{
    point_t* as_point = (point_t*)p;
    free(as_point);
}

// ----------------------------------------------------------------------------
// Test Cases

START_TEST(test_flat_map_new)
{
    flat_map_t* map1 = flat_map_new(4, delete_point);
    ck_assert(map1 != NULL);

    flat_map_delete(map1);

    // should fail, invalid page size
    flat_map_t* map2 = flat_map_new(3, delete_point);
    ck_assert(NULL == map2);

    // should fail, invalid deleter
    flat_map_t* map3 = flat_map_new(4, NULL);
    ck_assert(NULL == map3);
}
END_TEST

START_TEST(test_flat_map_insert)
{
    flat_map_t* map = flat_map_new(4, delete_point);
    ck_assert(map != NULL);

    point_t* p1 = make_point(1.0f, 1.0f);
    point_t* p2 = make_point(2.0f, 2.0f);
    point_t* p3 = make_point(3.0f, 3.0f);

    void* out1 = NULL;
    void* out2 = NULL;
    void* out3 = NULL;

    const bool r1 = flat_map_insert(map, 1, p1, &out1);
    ck_assert(r1);
    ck_assert(NULL == out1);

    const bool r2 = flat_map_insert(map, 2, p2, &out2);
    ck_assert(r2);
    ck_assert(NULL == out2);

    const bool r3 = flat_map_insert(map, 1, p3, &out3);
    ck_assert(r3);
    ck_assert(out3 != NULL);

    point_t* as_point = (point_t*)out3;

    // ensure the correct data was replaced
    ck_assert(as_point->x == 1.0f);
    ck_assert(as_point->y == 1.0f);

    delete_point(as_point);
    flat_map_delete(map);
}
END_TEST

START_TEST(test_flat_map_find)
{
    flat_map_t* map = flat_map_new(4, delete_point);
    ck_assert(map != NULL);

    point_t* p1 = make_point(1.0f, 1.0f);
    point_t* p2 = make_point(2.0f, 2.0f);
    point_t* p3 = make_point(3.0f, 3.0f);

    void* out1 = NULL;
    void* out2 = NULL;
    void* out3 = NULL;

    const bool r1 = flat_map_insert(map, 1, p1, &out1);
    ck_assert(r1);

    const bool r2 = flat_map_insert(map, 2, p2, &out2);
    ck_assert(r2);

    const bool r3 = flat_map_insert(map, 3, p3, &out3);
    ck_assert(r3);

    point_t* p1_out = (point_t*)flat_map_find(map, 1);
    point_t* p2_out = (point_t*)flat_map_find(map, 2);
    point_t* p3_out = (point_t*)flat_map_find(map, 3);

    ck_assert(p1_out != NULL);
    ck_assert(p2_out != NULL);
    ck_assert(p3_out != NULL);

    ck_assert(p1_out->x == 1.0f);
    ck_assert(p1_out->y == 1.0f);
    ck_assert(p2_out->x == 2.0f);
    ck_assert(p2_out->y == 2.0f);    
    ck_assert(p3_out->x == 3.0f);
    ck_assert(p3_out->y == 3.0f);

    ck_assert(flat_map_contains(map, 1));
    ck_assert(flat_map_contains(map, 2));
    ck_assert(flat_map_contains(map, 3));

    flat_map_delete(map);
}
END_TEST

START_TEST(test_flat_map_remove)
{
    flat_map_t* map = flat_map_new(4, delete_point);
    ck_assert(map != NULL);

    point_t* p1 = make_point(1.0f, 1.0f);
    point_t* p2 = make_point(2.0f, 2.0f);
    point_t* p3 = make_point(3.0f, 3.0f);

    void* out1 = NULL;
    void* out2 = NULL;
    void* out3 = NULL;

    const bool r1 = flat_map_insert(map, 1, p1, &out1);
    ck_assert(r1);

    const bool r2 = flat_map_insert(map, 2, p2, &out2);
    ck_assert(r2);

    const bool r3 = flat_map_insert(map, 3, p3, &out3);
    ck_assert(r3);

    point_t* p1_out = (point_t*)flat_map_find(map, 1);
    point_t* p2_out = (point_t*)flat_map_find(map, 2);
    point_t* p3_out = (point_t*)flat_map_find(map, 3);

    ck_assert(p1_out != NULL);
    ck_assert(p2_out != NULL);
    ck_assert(p3_out != NULL);

    ck_assert(flat_map_remove(map, 1));
    ck_assert(flat_map_remove(map, 2));
    ck_assert(flat_map_remove(map, 3));

    ck_assert(flat_map_find(map, 1) == NULL);
    ck_assert(flat_map_find(map, 2) == NULL);
    ck_assert(flat_map_find(map, 3) == NULL);

    flat_map_delete(map);
}
END_TEST

// ----------------------------------------------------------------------------
// Infrastructure
 
Suite* flat_map_suite(void)
{
    Suite* s = suite_create("flat-map");
    TCase* tc_core = tcase_create("flat-map-core");
    
    tcase_add_test(tc_core, test_flat_map_new);
    tcase_add_test(tc_core, test_flat_map_insert);
    tcase_add_test(tc_core, test_flat_map_find);
    tcase_add_test(tc_core, test_flat_map_remove);

    suite_add_tcase(s, tc_core);
    
    return s;
}

int main(void)
{
    Suite* suite = flat_map_suite();
    SRunner* runner = srunner_create(suite);

    srunner_run_all(runner, CK_NORMAL);    
    srunner_free(runner);
    
    return EXIT_SUCCESS;
}