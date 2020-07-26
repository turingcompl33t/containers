// intrusive_list.c
// Intrusive doubly-linked list.

#include "intrusive_list.h"

#include <stdlib.h>

// ----------------------------------------------------------------------------
// Exported

bool list_init(list_entry_t* head)
{
    if (NULL == head)
    {
        return false;
    }

    head->flink = head;
    head->blink = head;

    return true;
}

void list_push_front(list_entry_t* head, list_entry_t* entry)
{
    if (NULL == head || NULL == entry)
    {
        return;
    }

    if (list_is_empty(head))
    {
        head->blink = entry;
    }

    entry->flink = head->flink;
    entry->blink = head;

    head->flink->blink = entry;
    head->flink = entry;
}

void list_push_back(list_entry_t* head, list_entry_t* entry)
{
    if (NULL == head || NULL == entry)
    {
        return;
    }

    if (list_is_empty(head))
    {
        head->flink = entry;
    }

    entry->flink = head;
    entry->blink = head->blink;

    head->blink->flink = entry;
    head->blink = entry;
}

list_entry_t* list_pop_front(list_entry_t* head)
{
    if (NULL == head || list_is_empty(head))
    {
        return NULL;
    }

    list_entry_t* popped = head->flink;
    unlink_entry(popped);

    return popped;
}

list_entry_t* list_pop_back(list_entry_t* head)
{
    if (NULL == head || list_is_empty(head))
    {
        return NULL;
    }

    list_entry_t* popped = head->blink;
    unlink_entry(popped);

    return popped;
}

void list_remove_entry(list_entry_t* head, list_entry_t* entry)
{
    if (NULL == head || NULL == entry)
    {
        return;
    }

    unlink_entry(entry);
}

list_entry_t* list_find(list_entry_t* head, finder_f finder, void* ctx)
{
    if (NULL == head || NULL == finder || list_is_empty(head))
    {
        return NULL;
    }

    bool found = false;
    list_entry_t* current;
    for (current = head->flink; 
         current != head; 
         current = current->flink)
    {
        if (finder(current, ctx))
        {
            found = true;
            break;
        }
    }

    return found ? current : NULL;
}

list_entry_t* list_pop_front_if(
    list_entry_t* head, 
    predicate_f pred, 
    void* ctx)
{
    if (NULL == head || NULL == pred || list_is_empty(head))
    {
        return NULL;
    }

    list_entry_t* front = head->flink;

    if (pred(front, ctx))
    {
        unlink_entry(front);
        return front;
    }
    else
    {
        return NULL;
    }
}

list_entry_t* list_pop_back_if(
    list_entry_t* head, 
    predicate_f pred, 
    void* ctx)
{
    if (NULL == head || NULL == pred || list_is_empty(head))
    {
        return NULL;
    }

    list_entry_t* back = head->blink;

    if (pred(back, ctx))
    {
        unlink_entry(back);
        return back;
    }
    else
    {
        return NULL;
    }
}

// ----------------------------------------------------------------------------
// Internal

static void unlink_entry(list_entry_t* entry)
{
    entry->flink->blink = entry->blink;
    entry->blink->flink = entry->flink;
}

static bool list_is_empty(list_entry_t* head)
{
    return (head->flink == head) && (head->blink == head);
}