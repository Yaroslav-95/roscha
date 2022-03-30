#ifndef ROSCHA_OBJECT_H
#define ROSCHA_OBJECT_H

#include "hmap.h"
#include "slice.h"
#include "vector.h"

#include "sds/sds.h"

#include <stdbool.h>
#include <sys/types.h>

/* Types of roscha objects */
enum roscha_type {
	/* Only used internally; a variable that hasn't been set or defined. Does
	 * not have a correspoding field in the union.
	 */
	ROSCHA_NULL,
	/* an integer number. */
	ROSCHA_INT,
	/* Only used internally; a boolean value. */
	ROSCHA_BOOL,
	/* A text string */
	ROSCHA_STRING,
	/* A slice of a string; basically functions the same as a string */
	ROSCHA_SLICE,
	/* A vector of roscha objects */
	ROSCHA_VECTOR,
	/* A hashmap of roscha objects */
	ROSCHA_HMAP,
};

/* A reference counted object for use in the environment */
struct roscha_object {
	enum roscha_type type;
	size_t refcount;
	union {
		/*
		 * booleans are only used internally. It's not a bool so that we can
		 * evaluate as truthy objects such as integers and the other types
		 * without overflow problems of bool.
		 */
		uintptr_t boolean;
		/* integer numbers */
		int64_t integer;
		/* A dynamic string using the sds library */
		sds string;
		/* String slice */
		struct slice slice;
		/* vector of roscha_objects */
		struct vector *vector;
		/* hashmap of roscha_objects */
		struct hmap *hmap;
	};
};

/* Concatenate the textual representation of the object to an sds string */
sds roscha_object_string(const struct roscha_object *, sds str);

/* Return the textual representation of the type */
inline const char *roscha_type_print(enum roscha_type);

/* Create a new roscha object based on its type */
struct roscha_object *roscha_object_new_int(int64_t val);
struct roscha_object *roscha_object_new_string(sds str);
struct roscha_object *roscha_object_new_slice(struct slice);
struct roscha_object *roscha_object_new_vector(struct vector *);
struct roscha_object *roscha_object_new_hmap(struct hmap *);

#define roscha_object_new(v) _Generic((v), \
		int: roscha_object_new_int, \
		int64_t: roscha_object_new_int, \
		sds: roscha_object_new_string, \
		struct slice: roscha_object_new_slice, \
		struct vector *: roscha_object_new_vector, \
		struct hmap *: roscha_object_new_hmap \
		)(v)

/* Increment reference count of object */
void roscha_object_ref(struct roscha_object *);

/* Decrement reference count of object */
void roscha_object_unref(struct roscha_object *);

/*
 * Helper macro to create a roscha object wrapper and push to the vector in one
 * line.
 */
#define roscha_vector_push_new(vec, val) \
	vector_push(vec->vector, roscha_object_new(val))

/*
 * Helper function to push a value to a reference counted vector; increments the
 * count after adding the value to it.
 */
void roscha_vector_push(struct roscha_object *vec, struct roscha_object *val);

/*
 * Removes and returns the last value from a reference counted vector; doesn't
 * decrement the reference count since the value is returned.
 */
struct roscha_object *roscha_vector_pop(struct roscha_object *vec);

/*
 * Helper macro to create a roscha object wrapper and insert it to the hmap in
 * one line.
 */
#define roscha_hmap_set_new(h, k, v) hmap_set(h->hmap, k, roscha_object_new(v))

/*
 * Helper function to add a value to reference counted hmap; increments the
 * count after adding the value; returns the old value if it was present in the
 * hmap.
 */
struct roscha_object *roscha_hmap_sets(struct roscha_object *hmap, 
		struct slice key, struct roscha_object *value);

/* Same as roscha_hmap_sets but use a C string instead */
struct roscha_object *roscha_hmap_setstr(struct roscha_object *hmap, 
		const char *key, struct roscha_object *value);

#define roscha_hmap_set(h, k, v) _Generic((k), \
		char *: roscha_hmap_setstr, \
		struct slice: roscha_hmap_sets \
		)(h, k, v)

/*
 * Get a value from a reference counted hmap; the value's reference count is not
 * incremented, should be incremented by the receiver if needed.
 */
struct roscha_object *roscha_hmap_gets(struct roscha_object *hmap, 
		const struct slice *key);

/* Same as roscha_hmap_gets but use a C string instead */
struct roscha_object *roscha_hmap_getstr(struct roscha_object *hmap, 
		const char *key);

#define roscha_hmap_get(h, k) _Generic((k), \
		char *: roscha_hmap_getstr, \
		struct slice *: roscha_hmap_gets \
		)(h, k)

/*
 * Remove a value from a reference counted hmap; the value's reference count is
 * not decremented, since the value is returned.
 */
struct roscha_object *roscha_hmap_pops(struct roscha_object *hmap, 
		const struct slice *key);

/* Same as roscha_hmap_pops but use a C string instead */
struct roscha_object *roscha_hmap_popstr(struct roscha_object *hmap, 
		const char *key);

#define roscha_hmap_pop(h, k) _Generic((k), \
		char *: roscha_hmap_popstr, \
		struct slice *: roscha_hmap_pops \
		)(h, k)

/*
 * Remove a value from a reference counted hmap; the value's reference count is
 * decremented.
 */
void roscha_hmap_unsets(struct roscha_object *hmap, 
		const struct slice *key);

/* Same as roscha_hmap_unsets but use a C string instead */
void roscha_hmap_unsetstr(struct roscha_object *hmap, 
		const char *key);

#define roscha_hmap_unset(h, k) _Generic((k), \
		char *: roscha_hmap_unsetstr, \
		struct slice *: roscha_hmap_unsets \
		)(h, k)

#endif
