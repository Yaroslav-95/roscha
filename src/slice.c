#include "slice.h"

#include <stdlib.h>
#include <string.h>

struct slice
slice_new(const char *str, size_t start, size_t end)
{
	struct slice slice = {
		.str = str,
		.start = start,
		.end = end,
	};

	return slice;
}

void
slice_set(struct slice *slice, const char *str, size_t start, size_t end)
{
	slice->str = str;
	slice->start = start;
	slice->end = end;
}

size_t
slice_len(const struct slice *slice)
{
	return slice->end - slice->start;
}

int
slice_cmp(const struct slice *restrict a, const struct slice *restrict b)
{
	size_t lena = slice_len(a), lenb = slice_len(b);
	int lencmp = (lena > lenb) - (lena < lenb);
	if (lencmp) {
		return lencmp;
	}

	for (size_t i = 0; i < lena; i++) {
		char ca = a->str[a->start + i], cb = b->str[b->start + i];
		int cmp = (ca > cb) - (ca < cb);
		if (cmp) return cmp;
	}

	return 0;
}

void
slice_cpy(struct slice *dst, const struct slice *src)
{
	dst->str = src->str;
	dst->start = src->start;
	dst->end = src->end;
}

sds
slice_string(const struct slice *slice, sds str)
{
	size_t len = slice->end - slice->start;
	return sdscatlen(str, slice->str + slice->start, len);
}
