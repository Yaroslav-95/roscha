#ifndef ROSCHA_TOKEN_H
#define ROSCHA_TOKEN_H

#include "slice.h"

#include <stdbool.h>

enum token_type {
	TOKEN_ILLEGAL,
	TOKEN_EOF,
	/* Identifiers/Literals */
	TOKEN_IDENT,
	TOKEN_INT,
	TOKEN_STRING,
	/* Operators */
	TOKEN_ASSIGN,
	TOKEN_PLUS,
	TOKEN_MINUS,
	TOKEN_BANG,
	TOKEN_ASTERISK,
	TOKEN_SLASH,
	TOKEN_LT,
	TOKEN_GT,
	TOKEN_LTE,
	TOKEN_GTE,
	TOKEN_EQ,
	TOKEN_NOTEQ,
	/* Keyword-like operators */
	TOKEN_AND,
	TOKEN_OR,
	TOKEN_NOT,
	/* Delimiters */
	TOKEN_DOT,
	TOKEN_COMMA,
	TOKEN_LPAREN,
	TOKEN_RPAREN,
	TOKEN_LBRACE,
	TOKEN_RBRACE,
	TOKEN_LBRACKET,
	TOKEN_RBRACKET,
	TOKEN_POUND,
	TOKEN_PERCENT,
	/* Keywords */
	TOKEN_FOR,
	TOKEN_IN,
	TOKEN_BREAK,
	TOKEN_ENDFOR,
	TOKEN_TRUE,
	TOKEN_FALSE,
	TOKEN_IF,
	TOKEN_ELIF,
	TOKEN_ELSE,
	TOKEN_ENDIF,
	TOKEN_EXTENDS,
	TOKEN_BLOCK,
	TOKEN_ENDBLOCK,
	/* The document content */
	TOKEN_CONTENT,
};

/* A token in our template */
struct token {
	enum token_type type;
	struct slice literal;
	size_t line;
	size_t column;
};

/* Intialize our keywords hashmap */
void token_init_keywords(void);

/* Get the token type for a keyword, if it is a registered keyword. */
enum token_type token_lookup_ident(const struct slice *ident);

/* Return a C string with the token type name */
inline const char *token_type_print(enum token_type);

/* Concatenate this token to a sds string */
sds token_string(struct token *, sds str);

/* Free memory allocated by the keywords hashmap */
void token_free_keywords(void);

#endif
