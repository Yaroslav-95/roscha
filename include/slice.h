#ifndef CMONKEY_SLICE_H
#define CMONKEY_SLICE_H

#include "sds/sds.h"

#include <string.h>
#include <sys/types.h>

/* A slice of a C string */
struct slice {
	const char *str;
	size_t start;
	size_t end;
};

/* Create a new slice from an existing string indicating its bounds */
struct slice slice_new(const char *str, size_t start, size_t end);

/* Create a new slice from a string literal */
#define slice_whole(s) (struct slice){ s, 0, strlen(s), }

/* Set a slice to a new string and bounds */
void slice_set(struct slice *, const char *str, size_t start, size_t end);

/* Get the length of the slice */
size_t slice_len(const struct slice *);

/* Returns 0 if equal, 1 if a > b, -1 if a < b */
int slice_cmp(const struct slice *restrict a, const struct slice *restrict b);

/* Copy the slice from src to dst; dst should already be allocated */
void slice_cpy(struct slice *dst, const struct slice *src);

/* Concatenate the slice to an SDS string */
sds slice_string(const struct slice *, sds str);

#endif
