#include "token.h"

#include <stdio.h>
#include <stddef.h>

#include "hmap.h"

static struct hmap *keywords = NULL;
static const char *keys[] = {
	"and",
	"or",
	"not",
	"for",
	"in",
	"break",
	"endfor",
	"true",
	"false",
	"if",
	"elif",
	"else",
	"endif",
	"extends",
	"block",
	"endblock",
	NULL,
};
enum token_type values[] = {
	TOKEN_AND,
	TOKEN_OR,
	TOKEN_NOT,
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
	TOKEN_EOF,
};

const char *token_types[] = {
	"ILLEGAL",
	"EOF",
	/* Identifiers/Literals */
	"IDENTIFIER",
	"INTEGER",
	"STRING",
	/* Operators */
	"=",
	"+",
	"-",
	"!",
	"*",
	"/",
	"<",
	">",
	"<=",
	">=",
	"==",
	"!=",
	"and",
	"or",
	"not",
	/* Delimiters */
	".",
	",",
	"(",
	")",
	"{",
	"}",
	"[",
	"]",
	"#",
	"%",
	/* Keywords */
	"for",
	"in",
	"break",
	"endfor",
	"true",
	"false",
	"if",
	"elif",
	"else",
	"endif",
	"extends",
	"block",
	"endblock",
	/* The document content */
	"CONTENT",
};

void
token_init_keywords(void)
{
	if (keywords == NULL) {
		keywords = hmap_new();
		for (size_t i = 0; keys[i] != NULL; i++) {
			hmap_set(keywords, keys[i], values + i);
		}
	}

}

enum token_type
token_lookup_ident(const struct slice *ident)
{
	enum token_type *t = hmap_gets(keywords, ident);
	if (t) {
		return *t;
	}

	return TOKEN_IDENT;
}

extern inline const char *
token_type_print(enum token_type t)
{
	return token_types[t];
}

sds
token_string(struct token *token, sds str)
{
	const char *type = token_type_print(token->type);
	sds slicebuf = sdsempty();
	sdscatfmt(str, "TOKEN: type: %s, literal: %s", type,
			slice_string(&token->literal, slicebuf));
	sdsfree(slicebuf);
	return str;
}

void
token_free_keywords(void)
{
	hmap_free(keywords);
}
