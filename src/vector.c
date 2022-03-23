#include "vector.h"

static inline bool
vector_grow(struct vector *vec)
{
	vec->cap *= 2;
	vec->values = realloc(vec->values, sizeof(vec->values) * vec->cap);
	return vec->values != NULL;
}

struct vector *
vector_new_with_cap(size_t cap)
{
	struct vector *vec = malloc(sizeof(*vec));
	if (!vec) return NULL;
	vec->values = malloc(sizeof(vec->values) * cap);
	if (!vec->values) {
		free(vec);
		return NULL;
	}
	vec->cap = cap;
	vec->len = 0;

	return vec;
}

ssize_t
vector_push(struct vector *vec, void *val)
{
	if (vec->len >= vec->cap && !vector_grow(vec)) return -1;
	vec->values[vec->len] = val;
	vec->len++;

	return vec->len - 1;
}

void *
vector_pop(struct vector *vec)
{
	if (vec->len == 0) return NULL;
	return vec->values[--vec->len];
}

void
vector_free(struct vector *vec)
{
	free(vec->values);
	free(vec);
}
