#include "hmap.h"
#include "slice.h"

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>

#if SIZE_MAX == 0xFFFFFFFF
/* If size_t is 32bit */
static const size_t fnv_prime = 16777619u;
static const size_t fnv_offsetb = 2166136261u;
#elif SIZE_MAX == 0xFFFFFFFFFFFFFFFF
/* If size_t is 64bit */
static const size_t fnv_prime = 1099511628211u;
static const size_t fnv_offsetb = 14695981039346656037u;
#else
/* If size_t is 128bit. Maybe this is overdoing it? */
static const size_t fnv_prime = 309485009821345068724781371u;
static const size_t fnv_offsetb = 144066263297769815596495629667062367629u;
#endif

struct hnode {
    struct slice key;
    void *value;
    struct hnode *next;
};

struct hmap_iter {
	struct hmap *map;
	size_t index;
	size_t count;
	struct hnode *cur;
};

/* FNV1a */
static size_t
hash_slice(const struct slice *slice)
{
	size_t hash = fnv_offsetb;
	size_t i = slice->start;
	while (i < slice->end) {
		hash ^= slice->str[i];
		hash *= fnv_prime;
		i++;
	}

	return hash;
}

struct hmap *
hmap_new_with_cap(size_t cap)
{
	struct hmap *hm = malloc(sizeof *hm);
	hm->cap = cap;
	hm->size = 0;
	if (hm == NULL) return NULL;
	hm->buckets = calloc(cap, sizeof hm->buckets);
	if (hm->buckets == NULL) {
		free(hm);
		return NULL;
	}

	return hm;
}

void *
hmap_sets(struct hmap *hm, struct slice key, void *value)
{
    int pos = hash_slice(&key) % hm->cap;
    struct hnode *head = hm->buckets[pos];
    struct hnode *node = head;
    void *old_value = NULL;

    while (node) {
        if (slice_cmp(&node->key, &key) == 0) {
            old_value = node->value;
            node->value = value;
            return old_value;
        }
        node = node->next;
    }
    
    node = malloc(sizeof *node);
    node->key = key;
    node->value = value;
    node->next = head;
    hm->buckets[pos] = node;
    hm->size++;
    return old_value;
}

void *
hmap_gets(struct hmap *hm, const struct slice *key)
{
    size_t pos = hash_slice(key) % hm->cap;
    struct hnode *node = hm->buckets[pos];
    while (node != NULL) {
        if (slice_cmp(&node->key, key) == 0) {
            return node->value;
        }

        node = node->next;
    }

    return NULL;
}

void *
hmap_get(struct hmap *hm, const char *k)
{
	struct slice key = slice_whole(k);
	return hmap_gets(hm, &key);
}

void *
hmap_removes(struct hmap *hm, const struct slice *key)
{
    int pos = hash_slice(key) % hm->cap;
    struct hnode *node = hm->buckets[pos];
    struct hnode *prev = NULL;
    void *old_value;

    while (node) {
        if (slice_cmp(&node->key, key) == 0) {
            if (prev) {
                prev->next = node->next;
            } else {
                hm->buckets[pos] = node->next;
            }
            old_value = node->value;
            free(node);            
    		hm->size--;
            return old_value;
        }

        prev = node;
        node = node->next;
    }

    return NULL;
}

void *
hmap_remove(struct hmap *hm, const char *k)
{
	struct slice key = slice_whole(k);
	return hmap_removes(hm, &key);
}

#define HMAP_WALK(hm, ...) \
	struct hnode *node; \
	struct hnode *next; \
	for (size_t i = 0; i < hm->cap; i++) { \
		node = hm->buckets[i]; \
		while (node) { \
			next = node->next; \
			__VA_ARGS__; \
			node = next; \
		} \
	}

void
hmap_walk(struct hmap *hm, hmap_cb cb)
{
	HMAP_WALK(hm, cb(&node->key, node->value));
}

struct hmap_iter *
hmap_iter_new(struct hmap *hm)
{
	struct hmap_iter *iter = malloc(sizeof(*iter));
	iter->map = hm;
	iter->index = 0;
	iter->count = hm->size;
	iter->cur = NULL;

	return iter;
}

bool
hmap_iter_next(struct hmap_iter *iter, const struct slice **key, void **value)
{
	if (iter->count < 1) return false;

	if (!iter->cur || !iter->cur->next) {
		do {
			iter->cur = iter->map->buckets[iter->index++];
		} while (!iter->cur);
	} else {
		iter->cur = iter->cur->next;
	}
	*key = &iter->cur->key;
	*value = iter->cur->value;
	iter->count--;

	return true;
}

void
hmap_iter_free(struct hmap_iter *iter)
{
	free(iter);
}

void
hmap_destroy(struct hmap *hm, hmap_cb cb)
{
	HMAP_WALK(hm, cb(&node->key, node->value), free(node));

	free(hm->buckets);
	free(hm);
}

void
hmap_free(struct hmap *hm)
{
	HMAP_WALK(hm, free(node));

	free(hm->buckets);
	free(hm);
}
