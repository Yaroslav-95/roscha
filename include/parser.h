#ifndef ROSCHA_PARSER_H
#define ROSCHA_PARSER_H

#include "ast.h"
#include "hmap.h"
#include "lexer.h"
#include "token.h"
#include "vector.h"
#include "sds/sds.h"

struct parser {
	/* The name of the template; transfered to resulting template AST */
	char *name;
	/* The lexer that is ought to tokenize our input */
	struct lexer *lexer;
	/* Current token */
	struct token cur_token;
	/* Next token */
	struct token peek_token;
	/*
	 * Temporary field that holds {% block ... %} tags,  for easier/faster
	 * access to said blocks without having to traverse all the AST, in case the
	 * template is a child template. This hashmap will be transfered to the
	 * resulting AST upon finishing parsing.
	 */
	struct hmap *tblocks;
	/* vector of sds */
	struct vector *errors;
};

typedef struct expression *(*prefix_parse_f)(struct parser *);
typedef struct expression *(*infix_parse_f)(struct parser *, struct expression *);

/* Allocate a new parser */
struct parser *parser_new(char *name, char *input);

/* Parse template into an AST */
struct template *parser_parse_template(struct parser *);

/* Free all memory asociated with the parser */
void parser_destroy(struct parser *);

/*
 * Initialize variables needed for parsing; may be used by several parsers.
 */
void parser_init(void);

/*
 * Free all static memory related to parsing; called when parsing/evaluation is
 * no longer needed
 */
void parser_deinit(void);

#endif
