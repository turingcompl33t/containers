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

typedef bool (*finder_f)(list_entry_t*, void*);

bool list_init(list_entry_t* head);

void list_push_front(list_entry_t* head, list_entry_t* entry);

void list_push_back(list_entry_t* head, list_entry_t* entry);

list_entry_t* list_pop_front(list_entry_t* head);

list_entry_t* list_pop_back(list_entry_t* head);

void list_remove_entry(list_entry_t* head, list_entry_t* entry);

list_entry_t* list_find(list_entry_t* head, finder_f finder, void* ctx);

#endif // INTRUSIVE_LIST_H