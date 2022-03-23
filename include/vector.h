#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>

#define VEC_CAP 32

struct vector {
	size_t cap;
	size_t len;
	void **values;
};

struct vector *vector_new_with_cap(size_t cap);

#define vector_new() vector_new_with_cap(VEC_CAP)

ssize_t vector_push(struct vector *, void *val);

void *vector_pop(struct vector *);

void vector_free(struct vector *);

#define vector_foreach(vec, i, val) \
	for (i = 0, val = vec->values[i]; i < vec->len; i++, val = vec->values[i])

#endif
