// rcu_list.c
// A concurrent list utilizing the RCU algorithm.

#define _GNU_SOURCE
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

#include "rcu_list.h"

typedef struct list_node
{
    bool deleted;

    struct list_node* next;
    struct list_node* prev;
    
    void* data;
} list_node_t;

typedef struct zombie_node
{
    struct zombie_node* next;

    struct list_node*  zombie;
    struct rcu_handle* owner;
} zombie_node_t;

struct rcu_list
{
    // The exclusive lock acquired by writers.
    pthread_mutex_t write_lock;

    // The head and tail of the "live" list.
    list_node_t* head;
    list_node_t* tail;

    // The head of the zombie list.
    zombie_node_t* zombie_head;
};

// ----------------------------------------------------------------------------
// Internal

void rcu_read_lock(read_handle_t* handle);
void rcu_read_unlock(read_handle_t* handle);

void rcu_write_lock(write_handle_t* handle);
void rcu_write_unlock(write_handle_t* handle);

// ----------------------------------------------------------------------------
// Internal

static list_node_t* make_node(void* data);
static void destroy_node(list_node_t* node);

static zombie_node_t* make_zombie_node(void);
static void destroy_zombie_node(zombie_node_t* node);

static void lock_for_write(rcu_list_t* list);
static void unlock_for_write(rcu_list_t* list);

// ----------------------------------------------------------------------------
// Exported: List Interface

rcu_list_t* list_new(void)
{
    return list_new_with_deleter(free);
}

rcu_list_t* list_new_with_deleter(deleter_f deleter)
{
    if (NULL == deleter)
    {
        return NULL;
    }

    rcu_list_t* list = malloc(sizeof(rcu_list_t));
    if (NULL == list)
    {
        return NULL;
    }

    if (pthread_mutex_init(&list->write_lock, NULL) != 0)
    {
        free(list);
        return NULL;
    }

    list->head = NULL;
    list->tail = NULL;

    list->zombie_head = NULL;

    return list;
}

void list_delete(rcu_list_t* list)
{
    if (NULL == list)
    {
        return;
    }

    free(list);
}

void list_push_front(
    rcu_list_t*     list, 
    void*           data, 
    write_handle_t* handle)
{
    if (NULL == list)
    {
        return;
    }

    list_node_t* node = make_node(data);
    if (NULL == node)
    {
        return;
    }

    lock_for_write(list);

    list_node_t* old_head;
    __atomic_load(&list->head, &old_head, __ATOMIC_RELAXED);

    if (NULL == old_head)
    {
        // list is currently empty
        __atomic_store(&list->head, &node, __ATOMIC_SEQ_CST);
        __atomic_store(&list->tail, &node, __ATOMIC_SEQ_CST);
    }
    else
    {
        // general case
        __atomic_store(&node->next, &old_head, __ATOMIC_SEQ_CST);
        __atomic_store(&old_head->prev, &node, __ATOMIC_SEQ_CST);
        __atomic_store(&list->head, &node, __ATOMIC_SEQ_CST);
    }

    unlock_for_write(list);
}


void list_push_back(
    rcu_list_t*     list, 
    void*           data, 
    write_handle_t* handle)
{
    if (NULL == list)
    {
        return;
    }

    list_node_t* node = make_node(data);
    if (NULL == node)
    {
        return;
    }

    lock_for_write(list);

    list_node_t* old_tail;    
    __atomic_load(&list->tail, &old_tail, __ATOMIC_RELAXED);

    if (NULL == old_tail)
    {
        // list is currently empty
        __atomic_store(&list->head, &node, __ATOMIC_SEQ_CST);
        __atomic_store(&list->tail, &node, __ATOMIC_SEQ_CST);
    }
    else
    {
        // general case
        __atomic_store(&node->prev, &old_tail, __ATOMIC_SEQ_CST);
        __atomic_store(&old_tail->next, &node, __ATOMIC_SEQ_CST);
        __atomic_store(&list->tail, &node, __ATOMIC_SEQ_CST);
    }

    unlock_for_write(list);
}

void list_erase(
    rcu_list_t*     list, 
    iterator_t      iter, 
    write_handle_t* handle)
{
    if (NULL == list || NULL == iter.entry)
    {
        return;
    }

    lock_for_write(list);

    list_node_t* old_next;
    __atomic_load(&iter.entry->next, &old_next, __ATOMIC_RELAXED);

    // ensure the node has not already been marked for deletion
    if (!iter.entry->deleted)
    {
        iter.entry->deleted = true;

        list_node_t* old_prev;
        list_node_t* old_next;

        __atomic_load(&iter.entry->prev, &old_prev, __ATOMIC_SEQ_CST);
        __atomic_load(&iter.entry->next, &old_next, __ATOMIC_SEQ_CST);

        if (NULL == old_prev)
        {
            // no previous node, this is the head
            __atomic_store(&list->head, &old_next, __ATOMIC_SEQ_CST);
        }
        else
        {
            __atomic_store(&old_prev->next, &old_next, __ATOMIC_SEQ_CST);
        }

        if (NULL == old_next)
        {
            // no next node, this is the tail
            __atomic_store(&list->tail, &old_prev, __ATOMIC_SEQ_CST);
        }
        else
        {
            __atomic_store(&old_next->prev, &old_prev, __ATOMIC_SEQ_CST);
        }
    }
}

iterator_t list_find(
    rcu_list_t*    list, 
    void*          data, 
    finder_f       finder,
    read_handle_t* handle)
{
    // initialize an invalid iterator
    iterator_t iter = {
        .entry = NULL
    };

    if (NULL == list)
    {
        return iter;
    }

    list_node_t* current;
    __atomic_load(&list->head, &current, __ATOMIC_SEQ_CST);

    while (current != NULL)
    {
        if (finder(current->data, data))
        {
            iter.entry = current;
            break;
        }

        __atomic_load(&current->next, &current, __ATOMIC_SEQ_CST);
    }

    return iter;
}

iterator_t list_begin(
    rcu_list_t*    list, 
    read_handle_t* handle)
{
    iterator_t iter = {
        .entry = NULL
    };

    if (NULL == list)
    {
        return iter;
    }

    list_node_t* head;
    __atomic_load(&list->head, &head, __ATOMIC_SEQ_CST);

    iter.entry = head;
    return iter;
}

iterator_t list_end(
    rcu_list_t*    list, 
    read_handle_t* handle)
{
    iterator_t iter = {
        .entry = NULL
    };

    return iter;
}

// ----------------------------------------------------------------------------
// Exported: Iterator Interface

void* iterator_get(iterator_t* iter)
{
    return (NULL == iter) ? NULL : iter->entry->data;
}

// ----------------------------------------------------------------------------
// Exported: RCU Interface

read_handle_t list_register_reader(rcu_list_t* list)
{
    read_handle_t handle = {
        .list   = list,
        .zombie = NULL
    };
    return handle;
}

write_handle_t list_register_writer(rcu_list_t* list)
{
    write_handle_t handle = {
        .list   = list,
        .zombie = NULL
    };
    return handle;
}

// ----------------------------------------------------------------------------
// Exported: List Interface

void rcu_read_lock(read_handle_t* handle)
{
    zombie_node_t* z_node = make_zombie_node();
    
    z_node->owner  = handle;
    handle->zombie = z_node;

    rcu_list_t* list = handle->list;

    zombie_node_t* old_head;
    __atomic_load(&list->zombie_head, &old_head, __ATOMIC_SEQ_CST);

    do
    {
        __atomic_store(&z_node->next, &old_head, __ATOMIC_SEQ_CST);

    } while (!__atomic_compare_exchange(&list->zombie_head, &old_head, 
        &z_node, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
}

void rcu_read_unlock(read_handle_t* handle)
{
    zombie_node_t* z_node = handle->zombie;
    
    zombie_node_t* cached_next;
    __atomic_load(&z_node->next, &cached_next, __ATOMIC_SEQ_CST);

    bool last = true;

    // walk the zombie list to determine if this
    // is the last active reader in the list 
    zombie_node_t* n = cached_next;
    while (n != NULL)
    {
        list_node_t* owner;
        __atomic_load(&n->owner, &owner, __ATOMIC_SEQ_CST);

        if (owner != NULL)
        {
            // this is not the last active reader
            last = false;
            break;
        }

        __atomic_load(&n->next, &n, __ATOMIC_SEQ_CST);
    }

    n = cached_next;

    if (last)
    {
        while (n != NULL)
        {
            list_node_t* dead_node = n->zombie;
            if (dead_node != NULL)
            {
                destroy_node(dead_node);
            }

            zombie_node_t* old_node = n;

            __atomic_load(&n->next, &n, __ATOMIC_SEQ_CST);

            if (old_node != NULL)
            {
                destroy_zombie_node(old_node);
            }
        }

        __atomic_store(&z_node->next, &n, __ATOMIC_SEQ_CST);
    }

    void* null = NULL;
    __atomic_store(&z_node->owner, &null, __ATOMIC_SEQ_CST);
}

void rcu_write_lock(write_handle_t* handle)
{
    rcu_read_lock(handle);
}

void rcu_write_unlock(write_handle_t* handle)
{
    rcu_read_unlock(handle);
}

// ----------------------------------------------------------------------------
// Internal: General

static list_node_t* make_node(void* data)
{
    list_node_t* node = malloc(sizeof(list_node_t));
    if (NULL == node)
    {
        return NULL;
    }

    node->data    = data;
    node->next    = NULL;
    node->prev    = NULL;
    node->deleted = false;

    return node;
}

static void destroy_node(list_node_t* node)
{
    free(node);
}

static zombie_node_t* make_zombie_node(void)
{
    zombie_node_t* z_node = malloc(sizeof(zombie_node_t));
    if (NULL == z_node)
    {
        return NULL;
    }

    z_node->zombie = NULL;
    z_node->owner  = NULL;
    z_node->next   = NULL;

    return z_node;
}

static void destroy_zombie_node(zombie_node_t* node)
{
    free(node);
}

static void lock_for_write(rcu_list_t* list)
{
    pthread_mutex_lock(&list->write_lock);
}

static void unlock_for_write(rcu_list_t* list)
{
    pthread_mutex_unlock(&list->write_lock);
}