#define _POSIX_C_SOURCE 200809L
#include "tests/tests.h"
#include "roscha.h"

#include <string.h>

static void
check_env_errors(struct roscha_env *env, const char *file, int line,
		const char *func)
{
	struct vector *errors = roscha_env_check_errors(env);
	if (!errors) return;
	printf("\n");
	size_t i;
	sds val;
	vector_foreach (errors, i, val) {
		printf("parser error %lu: %s\n", i, val);
	}
	printf(TBLD TRED "FAIL!\n" TRST);
	printf("%s:%d: %s: ", file, line, func);
	printf("parser encountered errors\n");
	abort();
}

#define check_env_errors(p) \
	check_env_errors(p, __FILE__, __LINE__, __func__)

static void
test_eval_variable(void)
{
	char *input = "{{ foo.bar }}"
				  "{{ foo.bar + foo.baz }}"
				  "{{ foo.bar - foo.baz }}"
				  "{{ foo.bar * foo.baz }}"
				  "{{ foo.bar / foo.baz }}"
				  "{{ l }}"
				  "{{ l[0] }}, {{ l[1] }}"
				  "";
	char *expected = "8"
				  "12"
				  "4"
				  "32"
				  "2"
				  "[ hello, world, ]"
				  "hello, world"
				  "";
	struct roscha_object *foo = roscha_object_new(hmap_new());
	roscha_hmap_set_new(foo, "bar", 8);
	roscha_hmap_set_new(foo, "baz", 4);
	struct roscha_object *l = roscha_object_new(vector_new());
	roscha_vector_push_new(l, (slice_whole("hello")));
	roscha_vector_push_new(l, (slice_whole("world")));
	struct roscha_env *env = roscha_env_new();
	roscha_env_add_template(env, strdup("test"), input);
	check_env_errors(env);
	roscha_hmap_set(env->vars, "foo", foo);
	roscha_hmap_set(env->vars, "l", l);

	sds got = roscha_env_render(env, "test");
	check_env_errors(env);
	asserteq(strcmp(got, expected), 0);
	roscha_env_destroy(env);
	sdsfree(got);
	roscha_object_unref(foo);
	roscha_object_unref(l);
}

static void
test_eval_cond(void)
{
	char *input = "{% if foo > bar %}"
				  "Yes"
				  "{% elif baz %}"
				  "Maybe"
				  "{% else %}"
				  "No"
				  "{% endif %}";
	char *expected1 = "No";
	char *expected2 = "Maybe";
	char *expected3 = "Yes";
	struct roscha_object *foo = roscha_object_new(10);
	struct roscha_object *bar = roscha_object_new(20);
	struct roscha_object *baz = roscha_object_new(69);

	struct roscha_env *env = roscha_env_new();
	roscha_env_add_template(env, strdup("test"), input);
	check_env_errors(env);
	roscha_hmap_set(env->vars, "foo", foo);
	roscha_hmap_set(env->vars, "bar", bar);

	sds got = roscha_env_render(env, "test");
	check_env_errors(env);
	asserteq(strcmp(got, expected1), 0);
	sdsfree(got);

	roscha_hmap_set(env->vars, "baz", baz);
	got = roscha_env_render(env, "test");
	check_env_errors(env);
	asserteq(strcmp(got, expected2), 0);
	sdsfree(got);

	foo->integer = 420;
	got = roscha_env_render(env, "test");
	check_env_errors(env);
	asserteq(strcmp(got, expected3), 0);
	sdsfree(got);

	roscha_env_destroy(env);
	roscha_object_unref(foo);
	roscha_object_unref(bar);
	roscha_object_unref(baz);
}

static void
test_eval_loop(void)
{
	char *input = "{% for v in foo %}"
				  "{{ loop.index }}"
				  "{{ v }}"
				  "{% endfor %}";
	char *expected = "0hello1world";

	struct roscha_object *foo = roscha_object_new(vector_new());
	roscha_vector_push_new(foo, (slice_whole("hello")));
	roscha_vector_push_new(foo, (slice_whole("world")));

	struct roscha_env *env = roscha_env_new();
	roscha_env_add_template(env, strdup("test"), input);
	check_env_errors(env);
	roscha_hmap_set(env->vars, "foo", foo);
	sds got = roscha_env_render(env, "test");
	check_env_errors(env);
	asserteq(strcmp(got, expected), 0);

	sdsfree(got);
	roscha_env_destroy(env);
	roscha_object_unref(foo);
}

static void
test_eval_child(void)
{
	char *parent = "hello{% block title %}{% endblock %}"
				   "{% block content %}Content{% endblock %}"
				   "{% block foot %}Foot{% endblock %}";
	char *child = "{% extends \"parent\" %}"
				  "{% block title %}, world{% endblock %}"
				  "{% block content %}"
				  "In a beautiful place out in the country."
				  "{% endblock %}";
	char *expected = "hello, world"
					 "In a beautiful place out in the country."
					 "Foot";
	struct roscha_env *env = roscha_env_new();
	roscha_env_add_template(env, strdup("parent"), parent);
	check_env_errors(env);
	roscha_env_add_template(env, strdup("child"), child);
	check_env_errors(env);
	sds got = roscha_env_render(env, "child");
	check_env_errors(env);
	asserteq(strcmp(got, expected), 0);

	sdsfree(got);
	roscha_env_destroy(env);
}

static void
init(void)
{
	roscha_init();
}

static void
cleanup(void)
{
	roscha_deinit();
}

int
main(void)
{
	init();
	INIT_TESTS();
	RUN_TEST(test_eval_variable);
	RUN_TEST(test_eval_cond);
	RUN_TEST(test_eval_loop);
	RUN_TEST(test_eval_child);
	cleanup();
}
