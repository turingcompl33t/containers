// rcu_list.h
// A concurrent list utilizing the RCU algorithm.

#ifndef RCU_LIST_H
#define RCU_LIST_H

#include <stdbool.h>

typedef struct rcu_list rcu_list_t;

typedef struct iterator
{
    struct list_node* entry;
} iterator_t;

typedef struct rcu_handle 
{
    struct rcu_list*    list;
    struct zombie_node* zombie;
} rcu_handle_t;

typedef rcu_handle_t read_handle_t;
typedef rcu_handle_t write_handle_t;

typedef void (*deleter_f)(void*);

typedef bool (*finder_f)(void*, void*);

// ----------------------------------------------------------------------------
// Exported: List Interface

rcu_list_t* list_new(void);

rcu_list_t* list_new_with_deleter(deleter_f deleter);

void list_delete(rcu_list_t* list);

void list_push_front(rcu_list_t* list, void* data);

void list_push_back(rcu_list_t* list, void* data);

void list_erase(rcu_list_t* list, iterator_t iter);

iterator_t list_find(
    rcu_list_t* list, 
    void*       data, 
    finder_f    finder);

// ----------------------------------------------------------------------------
// Exported: Iterator Interface

void* iterator_get(iterator_t* iter);

// ----------------------------------------------------------------------------
// Exported: RCU Interface

read_handle_t list_register_reader(rcu_list_t* list);
write_handle_t list_register_writer(rcu_list_t* list);

// ----------------------------------------------------------------------------
// Exported: RCU Interface

void rcu_read_lock(read_handle_t* handle);
void rcu_read_unlock(read_handle_t* handle);

void rcu_write_lock(write_handle_t* handle);
void rcu_write_unlock(write_handle_t* handle);

#endif // RCU_LIST_H