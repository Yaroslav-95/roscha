#include "tests/tests.h"
#include "slice.h"

#include <string.h>

static void
test_slice_cmp(void)
{
	struct slice s1a = {
		.str = "hello world",
		.start = 6,
		.end = 11,
	};
	struct slice s1b = {
		.str = "world",
		.start = 0,
		.end = 5,
	};
	asserteq(slice_cmp(&s1a, &s1b), 0);
}

static void
test_slice_string(void)
{
	struct slice slice = {
		.str = "hello world",
		.start = 6,
		.end = 11,
	};
	sds str = sdsempty();
	slice_string(&slice, str);
	asserteq(strcmp("world", str), 0);
}

int
main(void)
{
	INIT_TESTS();
	RUN_TEST(test_slice_cmp);
	RUN_TEST(test_slice_string);
}
