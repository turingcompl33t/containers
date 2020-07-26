// intrusive_list.h
// Intrusive doubly-linked list.

#ifndef INTRUSIVE_LIST_H
#define INTRUSIVE_LIST_H

#include <stdbool.h>

typedef struct list_entry
{
    struct list_entry* flink;
    struct list_entry* blink;
} list_entry_t;

// The signature for the user-provided find predicate.
typedef bool (*finder_f)(list_entry_t*, void*);

// list_init()
//
// Initialize a new intrusive list.
bool list_init(list_entry_t* head);

// list_push_front()
//
// Push an entry onto the front of the intrusive list.
void list_push_front(list_entry_t* head, list_entry_t* entry);

// list_push_back()
//
// Push an entry onto the back of the intrusive list.
void list_push_back(list_entry_t* head, list_entry_t* entry);

// list_pop_front()
//
// Pop an entry off the front of the intrusive list.
list_entry_t* list_pop_front(list_entry_t* head);

// list_pop_back()
//
// Pop an entry off the back of the intrusive list.
list_entry_t* list_pop_back(list_entry_t* head);

// list_remove_entry()
//
// Unlink an entry from the intrusive list.
void list_remove_entry(list_entry_t* head, list_entry_t* entry);

// list_find()
//
// Find an entry in the intrusive list by predicate.
list_entry_t* list_find(
    list_entry_t* head, 
    finder_f      finder, 
    void*         ctx);

#endif // INTRUSIVE_LIST_H