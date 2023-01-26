#define _POSIX_C_SOURCE 200809L
#include "parser.h"
#include "tests/tests.h"

#include "ast.h"
#include "slice.h"

#include <string.h>

enum value_type {
	VALUE_IDENT,
	VALUE_INT,
	VALUE_BOOL,
	VALUE_STRING,
};

struct value {
	union {
		struct slice ident;
		struct slice string;
		int64_t      integer;
		bool         boolean;
	};
	enum value_type type;
};

static void
check_parser_errors(struct parser *parser, const char *file, int line,
                    const char *func)
{
	if (parser->errors->len > 0) {
		printf("\n");
		size_t i;
		sds    val;
		vector_foreach (parser->errors, i, val) {
			printf("parser error %lu: %s\n", i, val);
		}
		printf(TBLD TRED "FAIL!\n" TRST);
		printf("%s:%d: %s: ", file, line, func);
		printf("parser encountered errors\n");
		abort();
	}
}

#define check_parser_errors(p) \
	check_parser_errors(p, __FILE__, __LINE__, __func__)

static void
test_integer_literal(struct expression *expr, int64_t val)
{
	char         buf[128];
	struct slice sval;
	asserteq(expr->type, EXPRESSION_INT);
	asserteq(expr->integer.value, val);
	sprintf(buf, "%ld", val);
	sval = slice_whole(buf);
	asserteq(slice_cmp(&expr->token.literal, &sval), 0);
}

static void
test_identifier(struct expression *expr, struct slice *ident)
{
	asserteq(expr->type, EXPRESSION_IDENT);
	asserteq(slice_cmp(&expr->token.literal, ident), 0);
}

static void
test_boolean_literal(struct expression *expr, bool val)
{
	char        *str  = val ? "true" : "false";
	struct slice sval = slice_whole(str);
	asserteq(expr->type, EXPRESSION_BOOL);
	asserteq(expr->boolean.value, val);
	asserteq(slice_cmp(&expr->token.literal, &sval), 0);
}

static void
test_string_literal(struct expression *expr, const struct slice *val)
{
	asserteq(expr->type, EXPRESSION_STRING);
	asserteq(slice_cmp(&expr->string.value, val), 0);
}

static inline void
test_expected(struct expression *expr, struct value v)
{
	switch (v.type) {
	case VALUE_IDENT:
		test_identifier(expr, &v.ident);
		break;
	case VALUE_INT:
		test_integer_literal(expr, v.integer);
		break;
	case VALUE_BOOL:
		test_boolean_literal(expr, v.boolean);
		break;
	case VALUE_STRING:
		test_string_literal(expr, &v.string);
		break;
	}
}

#define VIDENT(v)                                    \
	(struct value)                                   \
	{                                                \
		.type = VALUE_IDENT, .ident = slice_whole(v) \
	}

#define VINT(v)                         \
	(struct value)                      \
	{                                   \
		.type = VALUE_INT, .integer = v \
	}

#define VBOOL(v)                         \
	(struct value)                       \
	{                                    \
		.type = VALUE_BOOL, .boolean = v \
	}

#define VSTR(v)                                        \
	(struct value)                                     \
	{                                                  \
		.type = VALUE_STRING, .string = slice_whole(v) \
	}

static inline void
test_infix(struct infix *expr, struct value lval, struct slice *op,
           struct value rval)
{
	test_expected(expr->left, lval);
	asserteq(slice_cmp(&expr->operator, op), 0);
	test_expected(expr->right, rval);
}

static inline void
test_literal_variables(void)
{
	struct {
		char        *input;
		struct value val;
	} tests[] = {
		{
			"{{ foo }}",
			VIDENT("foo"),
		},
		{
			"{{ 20 }}",
			VINT(20),
		},
		{
			"{{ true }}",
			VBOOL(true),
		},
		{
			"{{ false }}",
			VBOOL(false),
		},
		{0},
	};
	for (size_t i = 0; tests[i].input != NULL; i++) {
		struct parser   *parser = parser_new(strdup("test"), tests[i].input);
		struct template *tmpl   = parser_parse_template(parser);
		check_parser_errors(parser);
		assertneq(tmpl, NULL);
		asserteq(tmpl->blocks->len, 1);
		struct block *blk = tmpl->blocks->values[0];
		asserteq(blk->type, BLOCK_VARIABLE);
		test_expected(blk->variable.expression, tests[i].val);
		parser_destroy(parser);
		template_destroy(tmpl);
	}
}

static inline void
test_prefix_variables(void)
{
	struct {
		char        *input;
		struct slice operator;
		struct value val;
	} tests[] = {
		{
			"{{ !foo }}",
			slice_whole("!"),
			VIDENT("foo"),
		},
		{
			"{{ -bar }}",
			slice_whole("-"),
			VIDENT("bar"),
		},
		{
			"{{ -20 }}",
			slice_whole("-"),
			VINT(20),
		},
		{
			"{{ !true }}",
			slice_whole("!"),
			VBOOL(true),
		},
		{
			"{{ !false }}",
			slice_whole("!"),
			VBOOL(false),
		},
		{
			"{{ not false }}",
			slice_whole("not"),
			VBOOL(false),
		},
		{0},
	};
	for (size_t i = 0; tests[i].input != NULL; i++) {
		struct parser   *parser = parser_new(strdup("test"), tests[i].input);
		struct template *tmpl   = parser_parse_template(parser);
		check_parser_errors(parser);
		assertneq(tmpl, NULL);
		asserteq(tmpl->blocks->len, 1);
		struct block *blk = tmpl->blocks->values[0];
		asserteq(blk->type, BLOCK_VARIABLE);
		asserteq(blk->variable.expression->type, EXPRESSION_PREFIX);
		struct expression *pref = blk->variable.expression;
		asserteq(slice_cmp(&pref->prefix.operator, & tests[i].operator), 0);
		test_expected(pref->prefix.right, tests[i].val);
		parser_destroy(parser);
		template_destroy(tmpl);
	}
}

static inline void
test_infix_variables(void)
{
	struct {
		char        *input;
		struct value left;
		struct slice operator;
		struct value right;
	} tests[] = {
		{
			"{{ foo + bar }}",
			VIDENT("foo"),
			slice_whole("+"),
			VIDENT("bar"),
		},
		{
			"{{ 6 - 9 }}",
			VINT(6),
			slice_whole("-"),
			VINT(9),
		},
		{
			"{{ 4 * 20 }}",
			VINT(4),
			slice_whole("*"),
			VINT(20),
		},
		{
			"{{ foo / 20 }}",
			VIDENT("foo"),
			slice_whole("/"),
			VINT(20),
		},
		{
			"{{ \"str\" == \"str\" }}",
			VSTR("str"),
			slice_whole("=="),
			VSTR("str"),
		},
		{
			"{{ true != false }}",
			VBOOL(true),
			slice_whole("!="),
			VBOOL(false),
		},
		{
			"{{ 4 < 20 }}",
			VINT(4),
			slice_whole("<"),
			VINT(20),
		},
		{
			"{{ 4 <= 20 }}",
			VINT(4),
			slice_whole("<="),
			VINT(20),
		},
		{
			"{{ 100 > 20 }}",
			VINT(100),
			slice_whole(">"),
			VINT(20),
		},
		{
			"{{ 100 >= 20 }}",
			VINT(100),
			slice_whole(">="),
			VINT(20),
		},
		{
			"{{ true and true }}",
			VBOOL(true),
			slice_whole("and"),
			VBOOL(true),
		},
		{
			"{{ true or false }}",
			VBOOL(true),
			slice_whole("or"),
			VBOOL(false),
		},
		{0},
	};
	for (size_t i = 0; tests[i].input != NULL; i++) {
		struct parser   *parser = parser_new(strdup("test"), tests[i].input);
		struct template *tmpl   = parser_parse_template(parser);
		check_parser_errors(parser);
		assertneq(tmpl, NULL);
		asserteq(tmpl->blocks->len, 1);
		struct block *blk = tmpl->blocks->values[0];
		asserteq(blk->type, BLOCK_VARIABLE);
		asserteq(blk->variable.expression->type, EXPRESSION_INFIX);
		test_infix(&blk->variable.expression->infix,
		           tests[i].left, &tests[i].operator, tests[i].right);
		parser_destroy(parser);
		template_destroy(tmpl);
	}
}

static inline void
test_map_variables(void)
{
	char            *input  = "{{ map.key }}";
	struct value     left   = VIDENT("map");
	struct value     key    = VIDENT("key");
	struct parser   *parser = parser_new(strdup("test"), input);
	struct template *tmpl   = parser_parse_template(parser);
	check_parser_errors(parser);
	assertneq(tmpl, NULL);
	asserteq(tmpl->blocks->len, 1);
	struct block *blk = tmpl->blocks->values[0];
	asserteq(blk->type, BLOCK_VARIABLE);
	asserteq(blk->variable.expression->type, EXPRESSION_MAPKEY);
	struct expression *map = blk->variable.expression;
	test_expected(map->indexkey.left, left);
	test_expected(map->indexkey.key, key);
	parser_destroy(parser);
	template_destroy(tmpl);
}

static inline void
test_index_variables(void)
{
	char            *input  = "{{ arr[1 + 2] }}";
	struct value     left   = VIDENT("arr");
	struct value     ileft  = VINT(1);
	struct slice     iop    = slice_whole("+");
	struct value     iright = VINT(2);
	struct parser   *parser = parser_new(strdup("test"), input);
	struct template *tmpl   = parser_parse_template(parser);
	check_parser_errors(parser);

	assertneq(tmpl, NULL);
	asserteq(tmpl->blocks->len, 1);
	struct block *blk = tmpl->blocks->values[0];
	asserteq(blk->type, BLOCK_VARIABLE);
	asserteq(blk->variable.expression->type, EXPRESSION_INDEX);
	struct expression *map = blk->variable.expression;
	test_expected(map->indexkey.left, left);
	test_infix(&map->indexkey.key->infix, ileft, &iop, iright);

	parser_destroy(parser);
	template_destroy(tmpl);
}

static inline void
test_operator_precedence(void)
{
	struct {
		char *input;
		char *expected;
	} tests[] = {
		{
			"{{ -a * b }}",
			"{{ ((-a) * b) }}",
		},
		{
			"{{ 1 + 2 * 3 }}",
			"{{ (1 + (2 * 3)) }}",
		},
		{
			"{{ !-a }}",
			"{{ (!(-a)) }}",
		},
		{
			"{{ a + b + c }}",
			"{{ ((a + b) + c) }}",
		},
		{
			"{{ a + b - c }}",
			"{{ ((a + b) - c) }}",
		},
		{
			"{{ a * b * c }}",
			"{{ ((a * b) * c) }}",
		},
		{
			"{{ a * b / c }}",
			"{{ ((a * b) / c) }}",
		},
		{
			"{{ a + b / c }}",
			"{{ (a + (b / c)) }}",
		},
		{
			"{{ a + b * c + d / e - f }}",
			"{{ (((a + (b * c)) + (d / e)) - f) }}",
		},
		{
			"{{ 5 > 4 == 3 < 4 }}",
			"{{ ((5 > 4) == (3 < 4)) }}",
		},
		{
			"{{ 5 < 4 != 3 > 4 }}",
			"{{ ((5 < 4) != (3 > 4)) }}",
		},
		{
			"{{ 3 + 4 * 5 == 3 * 1 + 4 * 5 }}",
			"{{ ((3 + (4 * 5)) == ((3 * 1) + (4 * 5))) }}",
		},
		{
			"{{ (5 + 5) * 2 }}",
			"{{ ((5 + 5) * 2) }}",
		},
		{
			"{{ 2 / (5 + 5) }}",
			"{{ (2 / (5 + 5)) }}",
		},
		{
			"{{ (5 + 5) * 2 * (5 + 5) }}",
			"{{ (((5 + 5) * 2) * (5 + 5)) }}",
		},
		{
			"{{ -(5 + 5) }}",
			"{{ (-(5 + 5)) }}",
		},
		{
			"{{ foo[0] + bar[1 + 2] }}",
			"{{ (foo[0] + bar[(1 + 2)]) }}",
		},
		{
			"{{ foo.bar + foo.baz }}",
			"{{ (foo.bar + foo.baz) }}",
		},
		{
			"{{ foo.bar + bar[0].baz * foo.bar.baz }}",
			"{{ (foo.bar + (bar[0].baz * foo.bar.baz)) }}",
		},
		{0},
	};
	for (size_t i = 0; tests[i].input != NULL; i++) {
		struct parser   *parser = parser_new(strdup("test"), tests[i].input);
		struct template *tmpl   = parser_parse_template(parser);
		check_parser_errors(parser);
		assertneq(tmpl, NULL);
		asserteq(tmpl->blocks->len, 1);
		struct block *blk = tmpl->blocks->values[0];
		asserteq(blk->type, BLOCK_VARIABLE);
		sds output = block_string(blk, sdsempty());
		asserteq(strcmp(output, tests[i].expected), 0);
		sdsfree(output);
		parser_destroy(parser);
		template_destroy(tmpl);
	}
}

static inline void
test_loop_tag(void)
{
	char            *input  = "{% for v in seq %}"
							  "{% break %}"
							  "{% endfor %}"
							  "{{ foo }}";
	struct slice     item   = slice_whole("v");
	struct slice     seq    = slice_whole("seq");
	struct parser   *parser = parser_new(strdup("test"), input);
	struct template *tmpl   = parser_parse_template(parser);
	check_parser_errors(parser);

	assertneq(tmpl, NULL);
	asserteq(tmpl->blocks->len, 2);
	struct block *blk = tmpl->blocks->values[0];
	asserteq(blk->type, BLOCK_TAG);
	asserteq(blk->tag.type, TAG_FOR);
	asserteq(slice_cmp(&blk->tag.loop.item.token.literal, &item), 0);
	struct expression *seqexpr = blk->tag.loop.seq;
	asserteq(seqexpr->type, EXPRESSION_IDENT);
	asserteq(slice_cmp(&seqexpr->ident.token.literal, &seq), 0);
	asserteq(blk->tag.loop.subblocks->len, 2);
	struct block *sub1 = blk->tag.loop.subblocks->values[0];
	struct block *sub2 = blk->tag.loop.subblocks->values[1];
	asserteq(sub1->type, BLOCK_TAG);
	asserteq(sub2->type, BLOCK_TAG);
	asserteq(sub1->tag.type, TAG_BREAK);
	asserteq(sub2->tag.type, TAG_CLOSE);
	asserteq(sub2->tag.token.type, TOKEN_ENDFOR);

	struct block *extra_blk = tmpl->blocks->values[1];
	asserteq(extra_blk->type, BLOCK_VARIABLE);

	parser_destroy(parser);
	template_destroy(tmpl);
}

static inline void
test_cond_tag(void)
{
	char            *input    = "{% if false %}"
								"{{ foo }}"
								"{% elif 1 > 2 %}"
								"{{ bar }}"
								"{% else %}"
								"baz"
								"{% endif %}";
	struct value     ifexp    = VIDENT("foo");
	struct value     elifl    = VINT(1);
	struct slice     elifop   = slice_whole(">");
	struct value     elifr    = VINT(2);
	struct value     elifexp  = VIDENT("bar");
	struct slice     elsecont = slice_whole("baz");
	struct parser   *parser   = parser_new(strdup("test"), input);
	struct template *tmpl     = parser_parse_template(parser);
	check_parser_errors(parser);

	assertneq(tmpl, NULL);
	asserteq(tmpl->blocks->len, 1);
	struct block *blk = tmpl->blocks->values[0];
	asserteq(blk->type, BLOCK_TAG);
	asserteq(blk->tag.type, TAG_IF);

	struct branch *b1 = blk->tag.cond.root;
	assertneq(b1->condition, NULL);
	asserteq(b1->condition->type, EXPRESSION_BOOL);
	asserteq(b1->condition->token.type, TOKEN_FALSE);
	asserteq(b1->subblocks->len, 1);
	struct block *b1sub1 = b1->subblocks->values[0];
	asserteq(b1sub1->type, BLOCK_VARIABLE);
	test_expected(b1sub1->variable.expression, ifexp);

	struct branch *b2 = b1->next;
	assertneq(b2->condition, NULL);
	test_infix(&b2->condition->infix, elifl, &elifop, elifr);
	asserteq(b2->subblocks->len, 1);
	struct block *b2sub1 = b2->subblocks->values[0];
	asserteq(b2sub1->type, BLOCK_VARIABLE);
	test_expected(b2sub1->variable.expression, elifexp);

	struct branch *b3 = b2->next;
	asserteq(b3->condition, NULL);
	asserteq(b3->subblocks->len, 2);
	struct block *b3sub1 = b3->subblocks->values[0];
	asserteq(b3sub1->type, BLOCK_CONTENT);
	asserteq(slice_cmp(&b3sub1->content.token.literal, &elsecont), 0);
	struct block *b3sub2 = b3->subblocks->values[1];
	asserteq(b3sub2->tag.type, TAG_CLOSE);
	asserteq(b3sub2->tag.token.type, TOKEN_ENDIF);

	parser_destroy(parser);
	template_destroy(tmpl);
}

static inline void
test_parent_tag(void)
{
	char            *input  = "{% extends \"base.html\" %}";
	struct slice     name   = slice_whole("base.html");
	struct parser   *parser = parser_new(strdup("test"), input);
	struct template *tmpl   = parser_parse_template(parser);
	check_parser_errors(parser);

	assertneq(tmpl, NULL);
	asserteq(tmpl->blocks->len, 1);
	struct block *blk = tmpl->blocks->values[0];
	asserteq(blk->type, BLOCK_TAG);
	asserteq(blk->tag.type, TAG_EXTENDS);
	asserteq(slice_cmp(&blk->tag.parent.name->value, &name), 0);

	parser_destroy(parser);
	template_destroy(tmpl);
}

static inline void
test_tblock_tag(void)
{
	char            *input  = "{% block cock %}"
							  "{% endblock %}";
	struct slice     name   = slice_whole("cock");
	struct parser   *parser = parser_new(strdup("test"), input);
	struct template *tmpl   = parser_parse_template(parser);
	check_parser_errors(parser);

	assertneq(tmpl, NULL);
	asserteq(tmpl->blocks->len, 1);
	struct block *blk = tmpl->blocks->values[0];
	asserteq(blk->type, BLOCK_TAG);
	asserteq(blk->tag.type, TAG_BLOCK);
	asserteq(slice_cmp(&blk->tag.tblock.name.token.literal, &name), 0);
	asserteq(blk->tag.tblock.subblocks->len, 1);
	struct block *sub1 = blk->tag.tblock.subblocks->values[0];
	asserteq(sub1->type, BLOCK_TAG);
	asserteq(sub1->tag.type, TAG_CLOSE);
	asserteq(sub1->tag.token.type, TOKEN_ENDBLOCK);

	blk = hmap_gets(tmpl->tblocks, &name);
	assertneq(blk, NULL);
	asserteq(blk->type, BLOCK_TAG);
	asserteq(blk->tag.type, TAG_BLOCK);
	asserteq(slice_cmp(&blk->tag.tblock.name.token.literal, &name), 0);

	parser_destroy(parser);
	template_destroy(tmpl);
}

static void
init(void)
{
	parser_init();
}

static void
cleanup(void)
{
	parser_deinit();
}

int
main(void)
{
	init();
	INIT_TESTS();
	RUN_TEST(test_literal_variables);
	RUN_TEST(test_prefix_variables);
	RUN_TEST(test_infix_variables);
	RUN_TEST(test_map_variables);
	RUN_TEST(test_index_variables);
	RUN_TEST(test_operator_precedence);
	RUN_TEST(test_loop_tag);
	RUN_TEST(test_cond_tag);
	RUN_TEST(test_parent_tag);
	RUN_TEST(test_tblock_tag);
	cleanup();
}
