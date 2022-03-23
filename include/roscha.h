#ifndef ROSCHA_H
#define ROSCHA_H

#include "object.h"

/* The environment for evaluation templates */
struct roscha_env {
	/* Template variables; reference counted hmap of roscha objects */
	struct roscha_object *vars;
	/* vector of sds with error messages */
	struct vector *errors;
	/* internal */
	struct roscha_ *internal;
};

/*
 * Initialize variables needed for parsing; may be used by several roscha
 * parsers.
 */
void roscha_init(void);

/*
 * Free all static memory related to roscha; called when parsing/evaluation is
 * no longer needed
 */
void roscha_deinit(void);

/* Allocate a new environment */
struct roscha_env *roscha_env_new(void);

/*
 * Parse and add a template to the environment. Returns false upon encountering
 * a parsing error.
 */
bool roscha_env_add_template(struct roscha_env *, char *name, char *body);

/*
 * Load and parse templates from dir (non-recursively). All non-dir files are
 * read and parsed. Returns false if an error occurred.
 */
bool roscha_env_load_dir(struct roscha_env *, const char *path);

/* Render/evaluate the template */
sds roscha_env_render(struct roscha_env *, const char *name);

struct vector *roscha_env_check_errors(struct roscha_env *env);

/*
 * Free all memory associated with the environment, including parsed templates,
 * and reducing reference counts of objects.
 */
void roscha_env_destroy(struct roscha_env *);

#endif
