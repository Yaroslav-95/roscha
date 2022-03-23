#ifndef ROSCHA_LEXER_H
#define ROSCHA_LEXER_H

#include "slice.h"
#include "token.h"

#include <sys/types.h>
#include <stdbool.h>

/* The lexer */
struct lexer {
	/* Source input */
	const char *input;
	/* Length of input */
	size_t len;
	/* The current slice of the input string that will be tokenized */
	struct slice word;
	/* The current character belongs to content and should not be tokenized */
	bool in_content;
	size_t line;
	size_t column;
};

/* Allocate a new lexer with input as the source */
struct lexer *lexer_new(const char *input);

/* Get the next token from the lexer */
struct token lexer_next_token(struct lexer *);

/* Free all memory related to the lexer */
void lexer_destroy(struct lexer *);

#endif
