// hashmap_attr.c
// Hashmap attribute specification.

#include "hashmap_attr.h"

#include <stdlib.h>

static const float HASHMAP_ATTR_DEFAULT_LOAD_FACTOR = 0.75f;

static bool hashmap_attr_default_comparator(void* key_a, void* key_b);
static size_t hashmap_attr_default_keylen(void* key);
static void hashmap_attr_default_key_deleter(void* key);
static void hashmap_attr_default_value_deleter(void* value);

// ----------------------------------------------------------------------------
// Exported

hashmap_attr_t* hashmap_attr_new(void)
{
    hashmap_attr_t* attr = malloc(sizeof(hashmap_attr_t));
    if (NULL == attr)
    {
        return NULL;
    }

    attr->load_factor = 0.0f;

    attr->key_is_literal = false;

    attr->comparator    = NULL;
    attr->keylen        = NULL;
    attr->key_deleter   = NULL;
    attr->value_deleter = NULL;

    return attr;
}

hashmap_attr_t* hashmap_attr_default(void)
{
    hashmap_attr_t* attr = malloc(sizeof(hashmap_attr_t));
    if (NULL == attr)
    {
        return NULL;
    }

    attr->load_factor = HASHMAP_ATTR_DEFAULT_LOAD_FACTOR;

    attr->key_is_literal = true;

    attr->comparator    = hashmap_attr_default_comparator;
    attr->keylen        = hashmap_attr_default_keylen;
    attr->key_deleter   = hashmap_attr_default_key_deleter;
    attr->value_deleter = hashmap_attr_default_value_deleter;

    return attr;
}

void hashmap_attr_delete(hashmap_attr_t* attr)
{
    if (attr != NULL)
    {
        free(attr);
    }
}

// ----------------------------------------------------------------------------
// Internal

static bool hashmap_attr_default_comparator(void* key_a, void* key_b)
{
    size_t as_usize_a = (size_t)key_a;
    size_t as_usize_b = (size_t)key_b;
    return as_usize_a == as_usize_b;
}

static size_t hashmap_attr_default_keylen(void* key)
{
    return sizeof(void*);
}

static void hashmap_attr_default_key_deleter(void* key)
{
    return;
}

static void hashmap_attr_default_value_deleter(void* value)
{
    free(value);
}