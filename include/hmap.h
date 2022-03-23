#ifndef ROSCHA_HASHMAP_H
#define ROSCHA_HASHMAP_H
#include <stdbool.h>
#include <sys/types.h>

#include "slice.h"

#ifndef HASHMAP_CAP
#define HASHMAP_CAP 32
#endif

typedef void (hmap_cb)(const struct slice *key, void *value);

struct hmap {
    struct hnode **buckets;
    size_t cap;
    size_t size;
};

struct hmap_iter;

/* allocate a new hmap */
struct hmap *hmap_new_with_cap(size_t cap);

#define hmap_new() hmap_new_with_cap(HASHMAP_CAP)

/* 
 * Inserts a key-value pair into the map. Returns NULL if map did not have key,
 * old value if it did. 
 */
void *hmap_sets(struct hmap *hm, struct slice key, void *value);

/* Same as hmap_sets but pass a C string instead */
#define hmap_set(h, k, v) hmap_sets(h, slice_whole(k), v)

/* Returns a pointer to the value corresponding to the key. */
void *hmap_gets(struct hmap *hm, const struct slice *key);

/* Same as hmap_gets but pass a C string instead */
void *hmap_get(struct hmap *hm, const char *key);

/* 
 * Removes a key from the map, returning the value at the key if the key was
 * previously in the map.
 */
void *hmap_removes(struct hmap *hm, const struct slice *key);

/* Same as hmap_removes but pass a C string instead */
void *hmap_remove(struct hmap *hm, const char *key);

/* Iterate over keys in the hmap */
void hmap_walk(struct hmap *hm, hmap_cb);

/* Allocate a new hmap iterator */
struct hmap_iter *hmap_iter_new(struct hmap *);

/* Get the next key, value */
bool hmap_iter_next(struct hmap_iter *iter, const struct slice **key,
		void **value);

#define hmap_iter_foreach(it, k, v) while (hmap_iter_next(it, k, v))

/* Free a hmap iterator */
void hmap_iter_free(struct hmap_iter *iter);

/* free hmap related memory calling a function before freeing each node */
void hmap_destroy(struct hmap *hm, hmap_cb cb);

/* free hmap related memory */
void hmap_free(struct hmap *hm);

#endif
