// priority_queue.c
// Single-threaded priority queue.

#include "priority_queue.h"

#include <stdlib.h>

// ----------------------------------------------------------------------------
// Internal Declarations

typedef struct item
{
    struct item* next;
    void*        data;
} item_t;

struct queue
{
    // The head of the internal linked list.
    item_t* head;

    // The user-provided prioritization policy.
    prioritizer_f prioritizer;
};

static item_t* new_item(void* data);
static void destroy_item(item_t* item);

static bool queue_is_empty(queue_t* queue);

// ----------------------------------------------------------------------------
// Exported

queue_t* queue_new(prioritizer_f prioritizer)
{
    if (NULL == prioritizer)
    {
        return NULL;
    }

    queue_t* queue = malloc(sizeof(queue_t));
    if (NULL == queue)
    {
        return NULL;
    }

    queue->head        = NULL;
    queue->prioritizer = prioritizer;

    return queue;
}

void queue_delete(queue_t* queue)
{
    if (NULL == queue)
    {
        return;
    }

    free(queue);
}

bool queue_push(queue_t* queue, void* data)
{
    if (NULL == queue)
    {
        return false;
    }

    item_t* item = new_item(data);
    if (NULL == item)
    {
        return false;
    }

    item_t* prev = NULL;
    item_t* curr = queue->head;
    while (curr != NULL 
        && queue->prioritizer(curr->data, item->data))
    {
        prev = curr;
        curr = curr->next;
    }

    if (NULL == prev)
    {
        // broke on first iteration; this is highest priority item
        item->next = queue->head;
        queue->head = item;
    }
    else
    {
        prev->next = item;
        item->next = curr;
    }

    return true;
}

void* queue_pop(queue_t* queue)
{
    if (NULL == queue || queue_is_empty(queue))
    {
        return NULL;
    }

    item_t* popped = queue->head;

    queue->head = popped->next;

    void* data = popped->data;
    destroy_item(popped);

    return data;
}

void* queue_pop_if(queue_t* queue, predicate_f pred, void* ctx)
{
    if (NULL == queue || queue_is_empty(queue))
    {
        return NULL;
    }

    item_t* popped = queue->head;
    if (!pred(popped->data, ctx))
    {
        return NULL;
    }

    queue->head = popped->next;

    void* data = popped->data;
    destroy_item(popped);

    return data;
}

// ----------------------------------------------------------------------------
// Internal

static item_t* new_item(void* data)
{
    item_t* item = malloc(sizeof(item_t));
    if (NULL == item)
    {
        return NULL;
    }

    item->next = NULL;
    item->data = data;

    return item;
}

static void destroy_item(item_t* item)
{
    free(item);
}

static bool queue_is_empty(queue_t* queue)
{
    return (queue->head == NULL);
}