#ifndef ROSCHA_AST_H
#define ROSCHA_AST_H

#include "hmap.h"
#include "token.h"
#include "vector.h"

#include "sds/sds.h"

/* AST node structures */

enum block_type {
	BLOCK_CONTENT,
	BLOCK_VARIABLE,
	BLOCK_TAG,
};

enum tag_type {
	TAG_IF,
	TAG_FOR,
	TAG_BLOCK,
	TAG_EXTENDS,
	/* keyword-only tags */
	TAG_BREAK,
	TAG_CLOSE,
};

enum expression_type {
	EXPRESSION_IDENT,
	EXPRESSION_INT,
	EXPRESSION_BOOL,
	EXPRESSION_STRING,
	EXPRESSION_PREFIX,
	EXPRESSION_INFIX,
	EXPRESSION_MAPKEY,
	EXPRESSION_INDEX,
};

struct ident {
	struct token token;
};

struct integer {
	struct token token;
	int64_t value;
};

struct boolean {
	struct token token;
	bool value;
};

struct string {
	struct token token;
	struct slice value;
};

struct prefix {
	struct token token;
	struct slice operator;
	struct expression *right;
};

struct infix {
	struct token token;
	struct slice operator;
	struct expression *left;
	struct expression *right;
};

/* Either a map key (map.k) or an array/vector index (arr[i]) */
struct indexkey {
	struct token token;
	struct expression *left;
	struct expression *key;
};

struct expression {
	enum expression_type type;
	union {
		struct token token;
		struct ident ident;
		struct integer integer;
		struct boolean boolean;
		struct string string;
		struct prefix prefix;
		struct infix infix;
		struct indexkey indexkey;
	};
};

/* if, elif, else branch */
struct branch {
	struct token token;
	/* if condition is null it means it is an else branch */
	struct expression *condition;
	struct vector *subblocks;
	/* elif or else */
	struct branch *next;
};

/* start of if, elif, else */
struct cond {
	struct token token;
	struct branch *root;
};

/* for loop */
struct loop {
	struct token token;
	struct ident item;
	struct expression *seq;
	struct vector *subblocks;
};

/* template block {% block ... %} */
struct tblock {
	struct token token;
	struct ident name;
	struct vector *subblocks;
};

/* {% extends ... %} */
struct parent {
	struct token token;
	struct string *name;
};

/* {% ... %} blocks */
struct tag {
	union{
		struct token token;
		struct cond cond;
		struct loop loop;
		struct tblock tblock;
		struct parent parent;
	};
	enum tag_type type;
};

/* {{ ... }} blocks */
struct variable {
	struct token token;
	struct expression *expression;
};

/* blocks with content that doesn't need evaluation */
struct content {
	struct token token;
};

/*
 * The template is divided into blocks or chunks which are either plain text
 * content, {% %} tags or {{ }} variables. Not to be confused with 
 * {% block ... %} tags.
 */
struct block {
	union {
		struct token token;
		struct content content;
		struct tag tag;
		struct variable variable;
	};
	enum block_type type;
};

/* Root of the AST */
struct template {
	/*
	 * The name of the template, might be a file name; used to identifiy the
	 * template in error messages. Will be free'd by template_destroy function
	 * so a copy should be made if it is needed after destroying the AST.
	 */
	char *name;
	/*
	 * The source text of the template before parsing. Should be free'd manually
	 * by the caller of roscha_env_render.
	 */
	char *source;
	/*
	 * struct that holds references to {% block ... %} tags, for easier/faster
	 * access to said blocks. 
	 */
	struct hmap *tblocks;
	/*
	 * Holds a child template if there is one. Populated during evaluation,
	 * NULL'ed after evaluation, since a parent template can have different
	 * children depending on the context.
	 */
	struct template *child;
	/* vector of blocks */
	struct vector *blocks;
};

/* Concatenate to an SDS string a human friendly representation of the node */

sds expression_string(struct expression *, sds str);

sds tag_string(struct tag *, sds str);

sds variable_string(struct variable *, sds str);

sds content_string(struct content *, sds str);

sds block_string(struct block *, sds str);

sds template_string(struct template *, sds str);

/* Free all memory related with the objects */

void branch_destroy(struct branch *);

void tag_destroy(struct tag *);

void expression_destroy(struct expression *);

void block_destroy(struct block *);

void template_destroy(struct template *);

#endif
