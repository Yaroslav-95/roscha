#include "parser.h"
#include "ast.h"
#include "token.h"
#include "vector.h"

#include <stdio.h>
#include <stdlib.h>

enum precedence {
	PRE_LOWEST = 1,
	PRE_EQUALS,
	PRE_LG,
	PRE_SUM,
	PRE_PROD,
	PRE_PREFIX,
	PRE_CALL,
	PRE_INDEX,
};

static enum precedence precedence_values[] = {
	0,
	PRE_LOWEST,
	PRE_EQUALS,
	PRE_LG,
	PRE_SUM,
	PRE_PROD,
	PRE_PREFIX,
	PRE_CALL,
	PRE_INDEX,
};

static struct hmap *prefix_fns = NULL;
static struct hmap *infix_fns = NULL;
static struct hmap *precedences = NULL;

static struct block *parser_parse_block(struct parser *, struct block *opening);

static inline void
parser_register_prefix(enum token_type t, prefix_parse_f fn)
{
	hmap_set(prefix_fns, token_type_print(t), fn);
}

static inline void
parser_register_infix(enum token_type t, infix_parse_f fn)
{
	hmap_set(infix_fns, token_type_print(t), fn);
}

static inline void
parser_register_precedence(enum token_type t, enum precedence pre)
{
	hmap_set(precedences, token_type_print(t), &precedence_values[pre]);
}

static inline prefix_parse_f
parser_get_prefix(struct parser *parser, enum token_type t)
{
	return hmap_get(prefix_fns, token_type_print(t));
}

static inline infix_parse_f
parser_get_infix(struct parser *parser, enum token_type t)
{
	return hmap_get(infix_fns, token_type_print(t));
}

static inline enum precedence
parser_get_precedence(struct parser *parser, enum token_type t)
{
	enum precedence *pre = hmap_get(precedences, token_type_print(t));
	if (!pre) return PRE_LOWEST;
	return *pre;
}

static inline void
parser_next_token(struct parser *parser)
{
	parser->cur_token = parser->peek_token;
	parser->peek_token = lexer_next_token(parser->lexer);
}

static inline bool
parser_cur_token_is(struct parser *parser, enum token_type t)
{
	return parser->cur_token.type == t;
}

static inline bool
parser_peek_token_is(struct parser *parser, enum token_type t)
{
	return parser->peek_token.type == t;
}

static inline enum precedence
parser_peek_precedence(struct parser *parser)
{
	return parser_get_precedence(parser, parser->peek_token.type);
}

static inline enum precedence
parser_cur_precedence(struct parser *parser)
{
	return parser_get_precedence(parser, parser->cur_token.type);
}

#define parser_error(p, t, fmt, ...) \
	sds err = sdscatfmt(sdsempty(), "%s:%U:%U: "fmt, parser->name, \
			t.line, t.column, __VA_ARGS__); \
	vector_push(p->errors, err)

static inline void
parser_peek_error(struct parser *parser, enum token_type t)
{
	parser_error(parser, parser->peek_token, "expected token %s, got %s",
			token_type_print(t), token_type_print(parser->peek_token.type));
}

static inline bool
parser_expect_peek(struct parser *parser, enum token_type t)
{
	if (parser_peek_token_is(parser, t)) {
		parser_next_token(parser);
		return true;
	}
	parser_peek_error(parser, t);
	return false;
}

static inline void
parser_no_prefix_fn_error(struct parser *parser, enum token_type t)
{
	parser_error(parser, parser->cur_token, "%s not recognized as prefix",
	                    token_type_print(t));
}

static struct expression *
parser_parse_expression(struct parser *parser, enum precedence pre)
{
	prefix_parse_f prefix = parser_get_prefix(parser, parser->cur_token.type);
	if (!prefix) {
		parser_no_prefix_fn_error(parser, parser->cur_token.type);
		return NULL;
	}
	struct expression *lexpr = prefix(parser);

	while (pre < parser_peek_precedence(parser)
	       && !parser_peek_token_is(parser, TOKEN_PERCENT)
	       && !parser_peek_token_is(parser, TOKEN_RBRACE)) {
		infix_parse_f infix = parser_get_infix(parser, parser->peek_token.type);
		if (!infix) { return lexpr; }

		parser_next_token(parser);
		lexpr = infix(parser, lexpr);
	}

	return lexpr;
}

static struct expression *
parser_parse_identifier(struct parser *parser)
{
	struct expression *expr = malloc(sizeof(*expr));
	expr->type = EXPRESSION_IDENT;
	expr->token = parser->cur_token;

	return expr;
}

static struct expression *
parser_parse_integer(struct parser *parser)
{
	struct expression *expr = malloc(sizeof(*expr));
	expr->type = EXPRESSION_INT;
	expr->token = parser->cur_token;

	char *end;
	expr->integer.value =
		strtol(expr->token.literal.str + expr->token.literal.start, &end, 0);
	if (*end != '\0'
	    && (expr->token.literal.str + expr->token.literal.end) < end) {
		sds istr = slice_string(&expr->token.literal, sdsempty());
		parser_error(parser, parser->cur_token, "%s is not a valid integer",
				istr);
		sdsfree(istr);
		free(expr);
		return NULL;
	}

	return expr;
}

static struct expression *
parser_parse_boolean(struct parser *parser)
{
	struct expression *expr = malloc(sizeof(*expr));
	expr->type = EXPRESSION_BOOL;
	expr->token = parser->cur_token;
	expr->boolean.value = expr->token.type == TOKEN_TRUE;

	return expr;
}

static struct expression *
parser_parse_string(struct parser *parser)
{
	struct expression *expr = malloc(sizeof(*expr));
	expr->type = EXPRESSION_STRING;
	expr->token = parser->cur_token;
	expr->string.value = parser->cur_token.literal;
	expr->string.value.start++;
	expr->string.value.end--;

	return expr;
}

static struct expression *
parser_parse_grouped(struct parser *parser)
{
	parser_next_token(parser);
	struct expression *expr = parser_parse_expression(parser, PRE_LOWEST);
	if (!parser_expect_peek(parser, TOKEN_RPAREN)) { return NULL; }

	return expr;
}

static struct expression *
parser_parse_prefix(struct parser *parser)
{
	struct expression *expr = malloc(sizeof(*expr));
	expr->type = EXPRESSION_PREFIX;
	expr->token = parser->cur_token;
	expr->prefix.operator= parser->cur_token.literal;

	parser_next_token(parser);
	expr->prefix.right = parser_parse_expression(parser, PRE_PREFIX);

	return expr;
}

static struct expression *
parser_parse_infix(struct parser *parser, struct expression *lexpr)
{
	struct expression *expr = malloc(sizeof(*expr));
	expr->type = EXPRESSION_INFIX;
	expr->token = parser->cur_token;
	expr->infix.operator= parser->cur_token.literal;
	expr->infix.left = lexpr;

	enum precedence pre = parser_cur_precedence(parser);
	parser_next_token(parser);
	expr->infix.right = parser_parse_expression(parser, pre);

	return expr;
}

static struct expression *
parser_parse_mapkey(struct parser *parser, struct expression *lexpr)
{
	if (lexpr->type != EXPRESSION_IDENT 
			&& lexpr->type != EXPRESSION_MAPKEY
			&& lexpr->type != EXPRESSION_INDEX) {
		sds got = expression_string(lexpr, sdsempty());
		parser_error(parser, parser->cur_token,
				"expected a map identifier, key or index; got %s", got);
		sdsfree(got);
		return NULL;
	}
	struct expression *expr = malloc(sizeof(*expr));
	expr->type = EXPRESSION_MAPKEY;
	expr->token = parser->cur_token;
	expr->indexkey.left = lexpr;

	parser_next_token(parser);
	expr->indexkey.key = parser_parse_expression(parser, PRE_INDEX);
	if (expr->indexkey.key->type != EXPRESSION_IDENT) {
		sds got = expression_string(expr->indexkey.key, sdsempty());
		parser_error(parser, parser->cur_token,
				"expected a map key identifier, got %s", got);
		sdsfree(got);
		expression_destroy(expr);
		return NULL;
	}

	return expr;
}

static struct expression *
parser_parse_index(struct parser *parser, struct expression *lexpr)
{
	if (lexpr->type != EXPRESSION_IDENT
			&& lexpr->type != EXPRESSION_MAPKEY
			&& lexpr->type != EXPRESSION_INDEX) {
		sds got = expression_string(lexpr, sdsempty());
		parser_error(parser, parser->cur_token,
				"expected a vector identifier, key or index; got %s", got);
		sdsfree(got);
		return NULL;
	}
	struct expression *expr = malloc(sizeof(*expr));
	expr->type = EXPRESSION_INDEX;
	expr->token = parser->cur_token;
	expr->indexkey.left = lexpr;

	parser_next_token(parser);
	expr->indexkey.key = parser_parse_expression(parser, PRE_LOWEST);

	if (!parser_expect_peek(parser, TOKEN_RBRACKET)) {
		expression_destroy(expr);
		return NULL;
	}

	return expr;
}

static inline bool
parser_parse_loop(struct parser *parser, struct block *blk)
{
	blk->tag.type = TAG_FOR;

	if (!parser_expect_peek(parser, TOKEN_IDENT)) return false;
	blk->tag.loop.item.token = parser->cur_token;
	if (!parser_expect_peek(parser, TOKEN_IN)) return false;
	if (!parser_expect_peek(parser, TOKEN_IDENT)) return false;
	blk->tag.loop.seq = parser_parse_expression(parser, PRE_LOWEST);

	if (!parser_expect_peek(parser, TOKEN_PERCENT)) return false;
	if (!parser_expect_peek(parser, TOKEN_RBRACE)) return false;

	parser_next_token(parser);
	blk->tag.loop.subblocks = vector_new();
	while (!parser_cur_token_is(parser, TOKEN_EOF)) {
		struct block *subblk = parser_parse_block(parser, blk);
		if (subblk == NULL) {
			return false;
		}
		vector_push(blk->tag.loop.subblocks, subblk);
		parser_next_token(parser);
		if (subblk->type == BLOCK_TAG && subblk->tag.type == TAG_CLOSE) {
			break;
		}
	}

	return true;
}

static inline struct branch *
parser_parse_branch(struct parser *parser, struct block *opening)
{
	struct branch *brnch = calloc(1, sizeof(*brnch));
	brnch->token = parser->cur_token;

	if (brnch->token.type == TOKEN_IF || brnch->token.type == TOKEN_ELIF) {
		parser_next_token(parser);
		brnch->condition = parser_parse_expression(parser, PRE_LOWEST);
	}

	if (!parser_expect_peek(parser, TOKEN_PERCENT)) return false;
	if (!parser_expect_peek(parser, TOKEN_RBRACE)) return false;

	parser_next_token(parser);
	brnch->subblocks = vector_new();
	while (!parser_cur_token_is(parser, TOKEN_EOF)) {
		struct block *subblk = parser_parse_block(parser, opening);
		if (subblk == NULL) {
			branch_destroy(brnch);
			return NULL;
		}
		if (subblk->type == BLOCK_TAG && subblk->tag.type == TAG_IF) {
			brnch->next = subblk->tag.cond.root;
			free(subblk);
			break;
		}
		vector_push(brnch->subblocks, subblk);
		parser_next_token(parser);
		if (subblk->type == BLOCK_TAG && subblk->tag.type == TAG_CLOSE) {
			break;
		}
	}

	return brnch;
}

static inline bool
parser_parse_cond(struct parser *parser, struct block *blk)
{
	blk->tag.type = TAG_IF;
	blk->tag.cond.root = parser_parse_branch(parser, blk);
	if (!blk->tag.cond.root) {
		return false;
	}

	return true;
}

static inline bool
parser_parse_cond_alt(struct parser *parser, struct block *blk,
		struct block *opening)
{
	if (opening == NULL || opening->type != BLOCK_TAG
			|| opening->tag.type != TAG_IF) {
		parser_error(parser, parser->cur_token, "unexpected token %s",
				token_type_print(parser->cur_token.type));
		return false;
	}
	blk->tag.type = TAG_IF;
	blk->tag.cond.root = parser_parse_branch(parser, blk);
	if (!blk->tag.cond.root) {
		return false;
	}

	return true;
}

static inline bool
parser_parse_parent(struct parser *parser, struct block *blk)
{
	blk->tag.type = TAG_EXTENDS;
	if (!parser_expect_peek(parser, TOKEN_STRING)) return false;

	blk->tag.parent.name = malloc(sizeof(*blk->tag.parent.name));
	blk->tag.parent.name->token = parser->cur_token;
	blk->tag.parent.name->value = parser->cur_token.literal;
	blk->tag.parent.name->value.start++;
	blk->tag.parent.name->value.end--;

	if (!parser_expect_peek(parser, TOKEN_PERCENT)) return false;
	if (!parser_expect_peek(parser, TOKEN_RBRACE)) return false;

	return true;
}

static inline bool
parser_parse_tblock(struct parser *parser, struct block *blk)
{
	blk->tag.type = TAG_BLOCK;

	if (!parser_expect_peek(parser, TOKEN_IDENT)) return false;
	blk->tag.tblock.name.token = parser->cur_token;

	if (!parser_expect_peek(parser, TOKEN_PERCENT)) return false;
	if (!parser_expect_peek(parser, TOKEN_RBRACE)) return false;

	parser_next_token(parser);
	blk->tag.tblock.subblocks = vector_new();
	while (!parser_cur_token_is(parser, TOKEN_EOF)) {
		struct block *subblk = parser_parse_block(parser, blk);
		if (subblk == NULL) {
			return false;
		}
		vector_push(blk->tag.tblock.subblocks, subblk);
		parser_next_token(parser);
		if (subblk->type == BLOCK_TAG && subblk->tag.type == TAG_CLOSE) {
			break;
		}
	}

	hmap_sets(parser->tblocks, blk->tag.tblock.name.token.literal, blk);
	return true;
}

static inline struct block *
parser_parse_tag(struct parser *parser, struct block *opening)
{
	struct block *blk = malloc(sizeof(*blk));
	blk->type = BLOCK_TAG;

	parser_next_token(parser);
	parser_next_token(parser);

	blk->token = parser->cur_token;
	
	bool res = true;
	switch (parser->cur_token.type) {
	case TOKEN_FOR:
		res = parser_parse_loop(parser, blk);
		break;
	case TOKEN_BREAK:
		blk->tag.type = TAG_BREAK;
		goto onetoken;
	case TOKEN_IF:
		res = parser_parse_cond(parser, blk);
		break;
	case TOKEN_ELIF:
	case TOKEN_ELSE:
		res = parser_parse_cond_alt(parser, blk, opening);
		break;
	case TOKEN_EXTENDS:
		res = parser_parse_parent(parser, blk);
		break;
	case TOKEN_BLOCK:
		res = parser_parse_tblock(parser, blk);
		break;
	case TOKEN_ENDFOR:
		if (opening == NULL) goto noopening;
		if (opening->tag.type != TAG_FOR) goto noopening;
		goto closing;
	case TOKEN_ENDIF:
		if (opening == NULL) goto noopening;
		if (opening->tag.type != TAG_IF) goto noopening;
		goto closing;
	case TOKEN_ENDBLOCK:
		if (opening == NULL) goto noopening;
		if (opening->tag.type != TAG_BLOCK) goto noopening;
		goto closing;
	default:;
		parser_error(parser, parser->cur_token, "expected keyword, got %s",
				token_type_print(parser->cur_token.type));
		return NULL;
	}

	if (!res) {
		free(blk);
		return NULL;
	}

	return blk;
closing:
	blk->tag.type = TAG_CLOSE;
onetoken:
	if (!parser_expect_peek(parser, TOKEN_PERCENT)) goto fail;
	if (!parser_peek_token_is(parser, TOKEN_RBRACE)) goto fail;
	return blk;
noopening:;
	parser_error(parser, parser->cur_token, "unexpected closing tag %s",
				token_type_print(parser->cur_token.type));
fail:
	free(blk);
	return NULL;
}

static inline struct block *
parser_parse_variable(struct parser *parser)
{
	struct block *blk = malloc(sizeof(*blk));
	blk->type = BLOCK_VARIABLE;
	blk->token = parser->peek_token;

	parser_next_token(parser);
	parser_next_token(parser);

	blk->variable.expression = parser_parse_expression(parser, PRE_LOWEST);
	if (!blk->variable.expression) goto fail;
	if (!parser_expect_peek(parser, TOKEN_RBRACE)) goto fail;
	if (!parser_expect_peek(parser, TOKEN_RBRACE)) goto fail;

	return blk;
fail:
	block_destroy(blk);
	return NULL;
}

static inline struct block *
parser_parse_content(struct parser *parser)
{
	struct block *blk = malloc(sizeof(*blk));
	blk->type = BLOCK_CONTENT;
	blk->token = parser->cur_token;

	return blk;
}

static struct block *
parser_parse_block(struct parser *parser, struct block *opening)
{
	switch (parser->cur_token.type) {
	case TOKEN_LBRACE:
		switch (parser->peek_token.type) {
		case TOKEN_LBRACE:
			return parser_parse_variable(parser);
		case TOKEN_PERCENT:
			return parser_parse_tag(parser, opening);
		default:{
			parser_error(parser, parser->cur_token,
					"expected token %s or %s, got %s",
					token_type_print(TOKEN_LBRACE),
					token_type_print(TOKEN_PERCENT),
					token_type_print(parser->peek_token.type));
			return NULL;
		}
		}
	case TOKEN_CONTENT:
	default:
		return parser_parse_content(parser);
	}
}

struct parser *
parser_new(char *name, char *input)
{
	struct parser *parser = calloc(1, sizeof(*parser));
	parser->name = name;

	struct lexer *lex = lexer_new(input);
	parser->lexer = lex;

	parser->errors = vector_new();

	parser_next_token(parser);
	parser_next_token(parser);

	return parser;
}

struct template *
parser_parse_template(struct parser *parser)
{
	struct template *tmpl = malloc(sizeof(*tmpl));
	tmpl->name = parser->name;
	tmpl->source = (char *)parser->lexer->input;
	parser->tblocks = hmap_new();
	tmpl->child = NULL;
	tmpl->blocks = vector_new();

	while (!parser_cur_token_is(parser, TOKEN_EOF)) {
		struct block *blk = parser_parse_block(parser, NULL);
		if (blk == NULL) {
			break;
		}
		vector_push(tmpl->blocks, blk);

		parser_next_token(parser);
	}

	tmpl->tblocks = parser->tblocks;
	return tmpl;
}

void
parser_destroy(struct parser *parser)
{
	size_t i;
	char *val;
	vector_foreach(parser->errors, i, val) {
		sdsfree(val);
	}
	vector_free(parser->errors);
	lexer_destroy(parser->lexer);
	free(parser);
}

void
parser_init(void)
{
	token_init_keywords();

	prefix_fns = hmap_new();
	parser_register_prefix(TOKEN_IDENT, parser_parse_identifier);
	parser_register_prefix(TOKEN_INT, parser_parse_integer);
	parser_register_prefix(TOKEN_BANG, parser_parse_prefix);
	parser_register_prefix(TOKEN_MINUS, parser_parse_prefix);
	parser_register_prefix(TOKEN_NOT, parser_parse_prefix);
	parser_register_prefix(TOKEN_TRUE, parser_parse_boolean);
	parser_register_prefix(TOKEN_FALSE, parser_parse_boolean);
	parser_register_prefix(TOKEN_STRING, parser_parse_string);
	parser_register_prefix(TOKEN_LPAREN, parser_parse_grouped);
	parser_register_prefix(TOKEN_RPAREN, parser_parse_grouped);

	infix_fns = hmap_new();
	parser_register_infix(TOKEN_PLUS, parser_parse_infix);
	parser_register_infix(TOKEN_MINUS, parser_parse_infix);
	parser_register_infix(TOKEN_SLASH, parser_parse_infix);
	parser_register_infix(TOKEN_ASTERISK, parser_parse_infix);
	parser_register_infix(TOKEN_EQ, parser_parse_infix);
	parser_register_infix(TOKEN_NOTEQ, parser_parse_infix);
	parser_register_infix(TOKEN_LT, parser_parse_infix);
	parser_register_infix(TOKEN_GT, parser_parse_infix);
	parser_register_infix(TOKEN_LTE, parser_parse_infix);
	parser_register_infix(TOKEN_GTE, parser_parse_infix);
	parser_register_infix(TOKEN_AND, parser_parse_infix);
	parser_register_infix(TOKEN_OR, parser_parse_infix);

	parser_register_infix(TOKEN_DOT, parser_parse_mapkey);
	parser_register_infix(TOKEN_LBRACKET, parser_parse_index);

	precedences = hmap_new();
	parser_register_precedence(TOKEN_EQ, PRE_EQUALS);
	parser_register_precedence(TOKEN_NOTEQ, PRE_EQUALS);
	parser_register_precedence(TOKEN_LT, PRE_LG);
	parser_register_precedence(TOKEN_GT, PRE_LG);
	parser_register_precedence(TOKEN_LTE, PRE_LG);
	parser_register_precedence(TOKEN_GTE, PRE_LG);
	parser_register_precedence(TOKEN_AND, PRE_LG);
	parser_register_precedence(TOKEN_OR, PRE_LG);
	parser_register_precedence(TOKEN_PLUS, PRE_SUM);
	parser_register_precedence(TOKEN_MINUS, PRE_SUM);
	parser_register_precedence(TOKEN_ASTERISK, PRE_PROD);
	parser_register_precedence(TOKEN_SLASH, PRE_PROD);
	parser_register_precedence(TOKEN_DOT, PRE_INDEX);
	parser_register_precedence(TOKEN_LBRACKET, PRE_INDEX);
}

void
parser_deinit(void)
{
	token_free_keywords();
	hmap_free(infix_fns);
	hmap_free(prefix_fns);
	hmap_free(precedences);
}
