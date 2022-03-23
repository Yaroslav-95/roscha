#include "lexer.h"
#include "token.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static bool
isidentc(char c)
{
	return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static void
set_token(struct token *token, enum token_type t, const struct slice *s)
{
	token->type = t;
	if (s == NULL) {
		token->literal.str = "";
		token->literal.start = 0;
		token->literal.end = 0;
	} else {
		slice_cpy(&token->literal, s);
	}
}

static char
lexer_peek_prev_char(struct lexer *lexer)
{
	if (lexer->word.start <= 1) {
		return 0;
	}
	return lexer->input[lexer->word.start - 1];
}

static char
lexer_peek_char(struct lexer *lexer)
{
	if (lexer->word.start >= lexer->len) {
		return 0;
	}
	return lexer->input[lexer->word.start + 1];
}

static inline void
lexer_read_char(struct lexer *lexer)
{
	lexer->word.start = lexer->word.end;
	if (lexer->word.end > lexer->len) {
		lexer->word.end = 0;
		return;
	}
	char prevc = lexer_peek_prev_char(lexer);
	if (prevc == '\n') {
		lexer->line++;
		lexer->column = 0;
	}
	lexer->column++;
	lexer->word.end++;
}

static void
lexer_read_ident(struct lexer *lexer, struct token *token)
{
	size_t start = lexer->word.start;
	token->literal.str = lexer->input;
	while (isidentc(lexer->input[lexer->word.start]) 
			|| isdigit(lexer->input[lexer->word.start])) {
		lexer_read_char(lexer);
	}
	token->literal.start = start;
	token->literal.end = lexer->word.start;
}

static void
lexer_read_num(struct lexer *lexer, struct token *token)
{
	size_t start = lexer->word.start;
	token->literal.str = lexer->input;
	while (isdigit(lexer->input[lexer->word.start])) {
		lexer_read_char(lexer);
	}
	token->literal.start = start;
	token->literal.end = lexer->word.start;
}

static void
lexer_read_string(struct lexer *lexer, struct token *token)
{
	size_t start = lexer->word.start;
	token->literal.str = lexer->input;
	lexer_read_char(lexer);
	while(lexer->input[lexer->word.start] != '"' &&
			lexer->input[lexer->word.start] != '\0') {
		lexer_read_char(lexer);
	}
	lexer_read_char(lexer);
	token->literal.start = start;
	token->literal.end = lexer->word.start;
}

static void
lexer_read_content(struct lexer *lexer, struct token *token)
{
	size_t start = lexer->word.start;
	token->literal.str = lexer->input;
	while(lexer->input[lexer->word.start] != '{' &&
			lexer->input[lexer->word.start] != '\0') {
		lexer_read_char(lexer);
	}
	token->literal.start = start;
	token->literal.end = lexer->word.start;
}

static void
lexer_eatspace(struct lexer *lexer)
{
	while(isspace(lexer->input[lexer->word.start])) {
		lexer_read_char(lexer);
	}
}

struct lexer *
lexer_new(const char *input)
{
	struct lexer *lexer = malloc(sizeof(*lexer));
	lexer->input = input;
	lexer->len = strlen(lexer->input);
	lexer->word.str = lexer->input;
	lexer->word.start = 0;
	lexer->word.end = 0;
	lexer->in_content = true;
	lexer->line = 1;
	lexer->column = 0;
	lexer_read_char(lexer);

	return lexer;
}

struct token
lexer_next_token(struct lexer *lexer)
{
	struct token token = { .line = lexer->line, .column = lexer->column };
	char c = lexer->input[lexer->word.start];

	if (c == '\0') {
		set_token(&token, TOKEN_EOF, NULL);
		return token;
	}

	if (lexer->in_content && c != '{') {
		lexer_read_content(lexer, &token);
		token.type = TOKEN_CONTENT;
		return token;
	}

	lexer_eatspace(lexer);
	c = lexer->input[lexer->word.start];
	switch (c) {
	case '=':
		if (lexer_peek_char(lexer) == '=') {
			lexer->word.end++;
			set_token(&token, TOKEN_EQ, &lexer->word);
		} else {
			set_token(&token, TOKEN_ILLEGAL, &lexer->word);
		}
		break;
	case '+':
		set_token(&token, TOKEN_PLUS, &lexer->word);
		break;
	case '-':
		set_token(&token, TOKEN_MINUS, &lexer->word);
		break;
	case '!':
		if (lexer_peek_char(lexer) == '=') {
			lexer->word.end++;
			set_token(&token, TOKEN_NOTEQ, &lexer->word);
		} else {
			set_token(&token, TOKEN_BANG, &lexer->word);
		}
		break;
	case '/':
		set_token(&token, TOKEN_SLASH, &lexer->word);
		break;
	case '*':
		set_token(&token, TOKEN_ASTERISK, &lexer->word);
		break;
	case '<':
		if (lexer_peek_char(lexer) == '=') {
			lexer->word.end++;
			set_token(&token, TOKEN_LTE, &lexer->word);
		} else {
			set_token(&token, TOKEN_LT, &lexer->word);
		}
		break;
	case '>':
		if (lexer_peek_char(lexer) == '=') {
			lexer->word.end++;
			set_token(&token, TOKEN_GTE, &lexer->word);
		} else {
			set_token(&token, TOKEN_GT, &lexer->word);
		}
		break;
	case '(':
		set_token(&token, TOKEN_LPAREN, &lexer->word);
		break;
	case ')':
		set_token(&token, TOKEN_RPAREN, &lexer->word);
		break;
	case '.':
		set_token(&token, TOKEN_DOT, &lexer->word);
		break;
	case ',':
		set_token(&token, TOKEN_COMMA, &lexer->word);
		break;
	case '[':
		set_token(&token, TOKEN_LBRACKET, &lexer->word);
		break;
	case ']':
		set_token(&token, TOKEN_RBRACKET, &lexer->word);
		break;
	case '{':
		lexer->in_content = false;
		set_token(&token, TOKEN_LBRACE, &lexer->word);
		break;
	case '}':{
		char prevc = lexer_peek_prev_char(lexer);
		if (prevc == '}' || prevc == '%') {
			lexer->in_content = true;
		}
		set_token(&token, TOKEN_RBRACE, &lexer->word);
		break;
	 }
	case '%':
		set_token(&token, TOKEN_PERCENT, &lexer->word);
		break;
	default:
		if (c == '"') {
			lexer_read_string(lexer, &token);
			token.type = TOKEN_STRING;
			return token;
		} else if (isidentc(c)) {
			lexer_read_ident(lexer, &token);
			token.type = token_lookup_ident(&token.literal);
			return token;
		} else if (isdigit(c)) {
			lexer_read_num(lexer, &token);
			token.type = TOKEN_INT;
			return token;
		}
		set_token(&token, TOKEN_ILLEGAL, &lexer->word);
	}

	lexer_read_char(lexer);

	return token;
}

void
lexer_destroy(struct lexer *lexer)
{
	free(lexer);
}
