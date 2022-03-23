#ifndef TESTS_H
#define TESTS_H
#include <stdio.h>
#include <stdlib.h>

#ifndef NOCOLOR
#define TBLD "\033[1m"
#define TRED "\033[31m"
#define TGRN "\033[32m"
#define TBLU "\033[34m"
#define TRST "\033[0m"
#else
#define TBLD ""
#define TRED ""
#define TGRN ""
#define TBLU ""
#define TRST ""
#endif

#define RUN_TEST(test_func) \
	printf("%s:\t", #test_func); \
	fflush(stdout); \
	test_func(); \
	printf(TGRN "OK!\n" TRST)

#define INIT_TESTS() \
	printf(TBLD "running %s tests\n" TRST, __FILE__)

#define FAIL_TEST(reason) \
	printf(TBLD TRED "FAIL!\n" TRST); \
	printf("%s:%d: %s: ", __FILE__, __LINE__, __func__); \
	printf(reason); \
	abort()

#define asserteq(a, b) \
	if (a != b) { \
		FAIL_TEST("assertion " TBLD TBLU #a " == " #b TRST " failed\n"); \
	}

#define assertneq(a, b) \
	if (a == b) { \
		FAIL_TEST("assertion " TBLD TBLU #a " != " #b TRST " failed\n"); \
	}

#endif
