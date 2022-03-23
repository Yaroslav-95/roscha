#include "roscha.h"

#include "ast.h"
#include "hmap.h"
#include "vector.h"
#include "parser.h"

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define BUFSIZE 8912

struct roscha_ {
	/* hmap of template */
	struct hmap *templates;
	/* template currently being evaluated */
	const struct template *eval_tmpl;
	/* Set when a break tag was encountered */
	bool brk;
};

static struct roscha_object obj_null = {
	ROSCHA_NULL,
	0,
	.boolean = false,
};
static struct roscha_object obj_true = {
	ROSCHA_BOOL,
	0,
	.boolean = true,
};
static struct roscha_object obj_false = {
	ROSCHA_BOOL,
	0,
	.boolean = false,
};

static inline struct roscha_object *
get_bool_object(bool val)
{
	struct roscha_object *obj = val ? &obj_true : &obj_false;
	return obj;
}

static void
roscha_env_destroy_templates_cb(const struct slice *key, void *val)
{
	struct template *tmpl = val;
	template_destroy(tmpl);
}

#define eval_error(e, t, fmt, ...) \
	sds err = sdscatfmt(sdsempty(), "%s:%U:%U: "fmt, \
			e->internal->eval_tmpl->name, t.line, t.column, __VA_ARGS__); \
	vector_push(e->errors, err)

#define THERES_ERRORS env->errors->len > 0

static inline struct roscha_object *eval_expression(struct roscha_env *,
		struct expression *);

static inline struct roscha_object *
eval_prefix(struct roscha_env *env, struct prefix *pref)
{
	struct roscha_object *right = eval_expression(env, pref->right);
	if (!right) {
		return NULL;
	}
	struct roscha_object *res = NULL;
	switch (pref->token.type) {
	case TOKEN_BANG:
	case TOKEN_NOT:
		res = get_bool_object(!right->boolean);
		break;
	case TOKEN_MINUS:
		if (right->type != ROSCHA_INT) {
			eval_error(env, pref->token,
					"operator '%s' can only be used with integer types", 
					token_type_print(pref->token.type));
		} else {
			res = roscha_object_new(-right->integer);
		}
		break;
	default:{
		eval_error(env, pref->token, "invalid prefix operator '%s'", 
				token_type_print(pref->token.type));
		res = NULL;
	}
	}
	roscha_object_unref(right);

	return res;
}

static inline struct roscha_object *
eval_boolean_infix(struct roscha_env *env, struct token *op,
		struct roscha_object *left, struct roscha_object *right)
{
	struct roscha_object *res = NULL;
	switch (op->type) {
	case TOKEN_LT:
		res = get_bool_object(left->boolean < right->boolean);
		break;
	case TOKEN_GT:
		res = get_bool_object(left->boolean > right->boolean);
		break;
	case TOKEN_LTE:
		res = get_bool_object(left->boolean <= right->boolean);
		break;
	case TOKEN_GTE:
		res = get_bool_object(left->boolean >= right->boolean);
		break;
	case TOKEN_EQ:
		res = get_bool_object(left->boolean == right->boolean);
		break;
	case TOKEN_NOTEQ:
		res = get_bool_object(left->boolean != right->boolean);
		break;
	case TOKEN_AND:
		res = get_bool_object(left->boolean && right->boolean);
		break;
	case TOKEN_OR:
		res = get_bool_object(left->boolean || right->boolean);
		break;
	default:
		if (left->type != right->type) {
			eval_error(env, (*op), "types mismatch: %s %s %s",
					roscha_type_print(left->type), token_type_print(op->type),
					roscha_type_print(right->type));
			break;
		}
		eval_error(env, (*op), "bad operator: %s %s %s",
				roscha_type_print(left->type), token_type_print(op->type),
				roscha_type_print(right->type));
		break;
	}
	roscha_object_unref(left);
	roscha_object_unref(right);

	return res;
}

static inline struct roscha_object *
eval_integer_infix(struct roscha_env *env, struct token *op,
		struct roscha_object *left, struct roscha_object *right)
{
	struct roscha_object *res;
	switch (op->type) {
	case TOKEN_PLUS:
		res = roscha_object_new(left->integer + right->integer);
		break;
	case TOKEN_MINUS:
		res = roscha_object_new(left->integer - right->integer);
		break;
	case TOKEN_ASTERISK:
		res = roscha_object_new(left->integer * right->integer);
		break;
	case TOKEN_SLASH:
		res = roscha_object_new(left->integer / right->integer);
		break;
	default:
		return eval_boolean_infix(env, op, left, right);
	}
	roscha_object_unref(left);
	roscha_object_unref(right);

	return res;
}

static inline struct roscha_object *
eval_infix(struct roscha_env *env, struct infix *inf)
{
	struct roscha_object *left = eval_expression(env, inf->left);
	if (!left) {
		return NULL;
	}
	struct roscha_object *right = eval_expression(env, inf->right);
	if (!right) {
		roscha_object_unref(left);
		return NULL;
	}
	if (left->type == ROSCHA_INT && right->type == ROSCHA_INT)
	{
		return eval_integer_infix(env, &inf->token, left, right);
	}

	return eval_boolean_infix(env, &inf->token, left, right);
}

static inline struct roscha_object *
eval_mapkey(struct roscha_env *env, struct indexkey *mkey)
{
	struct roscha_object *res = NULL;
	struct roscha_object *map = eval_expression(env, mkey->left);
	if (!map) return NULL;
	if (map->type != ROSCHA_HMAP) {
		eval_error(env, mkey->token, "expected %s type got %s",
				roscha_type_print(ROSCHA_HMAP),
				roscha_type_print(map->type));
		goto out;
	}
	if (mkey->key->type != EXPRESSION_IDENT) {
		eval_error(env, mkey->key->token, "bad map key '%s'",
				token_type_print(mkey->key->token.type));
		goto out;
	}
	res = hmap_gets(map->hmap, &mkey->key->token.literal);
	if (!res) {
		res = &obj_null;
	} else {
		roscha_object_ref(res);
	}

out:
	roscha_object_unref(map);
	return res;
}

static inline struct roscha_object *
eval_index(struct roscha_env *env, struct indexkey *index)
{
	struct roscha_object *res = NULL;
	struct roscha_object *vec = eval_expression(env, index->left);
	if (!vec) return NULL;
	if (vec->type != ROSCHA_VECTOR) {
		eval_error(env, index->token, "expected %s type got %s",
				roscha_type_print(ROSCHA_VECTOR),
				roscha_type_print(vec->type));
		goto out;
	}
	struct roscha_object *i = eval_expression(env, index->key);
	if (i->type != ROSCHA_INT) {
		eval_error(env, index->key->token, "bad vector key type %s",
				roscha_type_print(ROSCHA_INT));
		goto out2;
	}
	if (i->integer > (vec->vector->len - 1)) {
		res = &obj_null;
	} else {
		res = vec->vector->values[i->integer];
		roscha_object_ref(res);
	}

out2:
	roscha_object_unref(i);
out:
	roscha_object_unref(vec);
	return res;
}

static inline struct roscha_object *
eval_expression(struct roscha_env *env, struct expression *expr)
{
	struct roscha_object *obj = NULL;
	switch (expr->type) {
	case EXPRESSION_IDENT:
		obj = roscha_hmap_get(env->vars, &expr->ident.token.literal);
		if (!obj) {
			obj = &obj_null;
		} else {
			roscha_object_ref(obj);
		}
		break;
	case EXPRESSION_INT:
		obj = roscha_object_new(expr->integer.value);
		break;
	case EXPRESSION_BOOL:
		obj = get_bool_object(expr->boolean.value);
		break;
	case EXPRESSION_STRING:
		obj = roscha_object_new(expr->string.value);
		break;
	case EXPRESSION_PREFIX:
		obj = eval_prefix(env, &expr->prefix);
		break;
	case EXPRESSION_INFIX:
		obj = eval_infix(env, &expr->infix);
		break;
	case EXPRESSION_MAPKEY:
		obj = eval_mapkey(env, &expr->indexkey);
		break;
	case EXPRESSION_INDEX:
		obj = eval_index(env, &expr->indexkey);
		break;
	}

	return obj;
}

static inline sds
eval_variable(struct roscha_env *env, sds r, struct variable *var)
{
	struct roscha_object *obj = eval_expression(env, var->expression);
	if (!obj) {
		return r;
	}
	r = roscha_object_string(obj, r);
	roscha_object_unref(obj);

	return r;
}

static inline sds eval_subblocks(struct roscha_env *, sds r,
		struct vector *blks);

static inline sds
eval_branch(struct roscha_env *env, sds r, struct branch *br)
{
	if (br->condition) {
		struct roscha_object *cond = eval_expression(env, br->condition);
		if (cond->boolean) {
			r = eval_subblocks(env, r, br->subblocks);
		} else if (br->next) {
			r = eval_branch(env, r, br->next);
		}
		roscha_object_unref(cond);
	} else {
		r = eval_subblocks(env, r, br->subblocks);
	}
	return r;
}

static inline sds
eval_loop(struct roscha_env *env, sds r, struct loop *loop)
{
	struct roscha_object *loopv = roscha_object_new(hmap_new());
	struct roscha_object *indexv = roscha_object_new(0);
	roscha_hmap_set(loopv, "index", indexv);
	struct slice loopk = slice_whole("loop");
	struct roscha_object *outerloop = roscha_hmap_set(env->vars, "loop", loopv);
	struct slice it = loop->item.token.literal;
	struct roscha_object *outeritem = roscha_hmap_get(env->vars, &it);
	struct roscha_object *seq = eval_expression(env, loop->seq);
	if (!seq) return r;
	if (seq->type == ROSCHA_VECTOR) {
		struct roscha_object *item;
		vector_foreach(seq->vector, indexv->integer, item) {
			roscha_hmap_set(env->vars, it, item);
			r = eval_subblocks(env, r, loop->subblocks);
			roscha_hmap_unset(env->vars, &it);
			if (THERES_ERRORS) break;
			if (env->internal->brk) {
				env->internal->brk = false;
				break;
			}
		}
	} else if(seq->type == ROSCHA_HMAP) {
		struct hmap_iter *iter = hmap_iter_new(seq->hmap);
		const struct slice *key;
		void *val;
		hmap_iter_foreach(iter, &key, &val) {
			struct roscha_object *item = val;
			indexv->integer++;
			roscha_hmap_set(env->vars, it, item);
			r = eval_subblocks(env, r, loop->subblocks);
			roscha_hmap_unset(env->vars, &it);
			if (THERES_ERRORS) break;
			if (env->internal->brk) {
				env->internal->brk = false;
				break;
			}
		}
	} else {
		eval_error(env, loop->seq->token,
				"sequence should be of type %s or %s, got %s",
				roscha_type_print(ROSCHA_VECTOR),
				roscha_type_print(ROSCHA_HMAP), roscha_type_print(seq->type));
	}

	if (outerloop) {
		roscha_hmap_set(env->vars, loopk, outerloop);
		roscha_object_unref(outerloop);
		roscha_object_unref(loopv);
	} else {
		roscha_hmap_unset(env->vars, &loopk);
	}
	if (outeritem) {
		roscha_hmap_set(env->vars, it, outeritem);
		roscha_object_unref(outeritem);
	}
	roscha_object_unref(indexv);
	roscha_object_unref(loopv);
	roscha_object_unref(seq);

	return r;
}

static inline struct tblock *
get_child_tblock(struct roscha_env *env, struct slice *name,
		const struct template *tmpl)
{
	struct tblock *tblk = NULL;
	if (tmpl->child) {
		tblk = get_child_tblock(env, name, tmpl->child);
	}
	if (!tblk) {
		return hmap_gets(tmpl->tblocks, name);
	}

	return tblk;
}

static inline sds
eval_tblock(struct roscha_env *env, sds r, struct tblock *tblk)
{
	struct tblock *child = get_child_tblock(env, &tblk->name.token.literal,
			env->internal->eval_tmpl);
	if (child) {
		tblk = child;
	}

	return eval_subblocks(env, r, tblk->subblocks);
}

static inline sds
eval_tag(struct roscha_env *env, sds r, struct tag *tag)
{
	switch (tag->type) {
	case TAG_IF:
		return eval_branch(env, r, tag->cond.root);
	case TAG_FOR:
		return eval_loop(env, r, &tag->loop);
	case TAG_BLOCK:
		return eval_tblock(env, r, &tag->tblock);
	case TAG_EXTENDS:{
		eval_error(env, tag->token, "extends tag can only be the first tag",
				tag->token);
		break;
	}
	case TAG_BREAK:
		env->internal->brk = true;
		break;
	default:
		break;
	}

	return r;
}

static inline sds
eval_block(struct roscha_env *env, sds r, struct block *blk)
{
	switch (blk->type) {
		case BLOCK_CONTENT:
			return slice_string(&blk->token.literal, r);
		case BLOCK_VARIABLE:
			return eval_variable(env, r, &blk->variable);
		case BLOCK_TAG:
			return eval_tag(env, r, &blk->tag);
	}
}

static inline sds
eval_subblocks(struct roscha_env *env, sds r, struct vector *blks)
{
	size_t i;
	struct block *blk;
	vector_foreach (blks, i, blk) {
		r = eval_block(env, r, blk);
		if (THERES_ERRORS) return r;
		if (env->internal->brk) return r;
	}

	return r;
}

static inline sds
eval_template(struct roscha_env *env, const struct slice *name,
		struct template *child)
{
	struct template *tmpl = hmap_gets(env->internal->templates, name);
	if (!tmpl) {
		sds errmsg = sdscat(sdsempty(), "template \"");
		errmsg = slice_string(name, errmsg);
		errmsg = sdscat(errmsg, "\" not found");
		vector_push(env->errors, errmsg);
		return NULL;
	}

	tmpl->child = child;
	env->internal->eval_tmpl = tmpl;

	struct block *blk = tmpl->blocks->values[0];
	if (blk->type == BLOCK_TAG && blk->tag.type == TAG_EXTENDS) {
		struct slice name = blk->tag.parent.name->value;
		return eval_template(env, &name, tmpl);
	}

	sds r = sdsempty();
	r = eval_subblocks(env, r, tmpl->blocks);

	env->internal->eval_tmpl = NULL;

	return r;
}

void
roscha_init(void)
{
	parser_init();
}

void
roscha_deinit(void)
{
	parser_deinit();
}

struct roscha_env *
roscha_env_new(void)
{
	struct roscha_env *env = calloc(1, sizeof(*env));
	env->internal = calloc(1, sizeof(*env->internal));
	env->vars = roscha_object_new(hmap_new());
	env->internal->templates = hmap_new();
	env->errors = vector_new();

	return env;
}

bool
roscha_env_add_template(struct roscha_env *env, char *name, char *body)
{
	struct parser *parser = parser_new(name, body);
	struct template *tmpl = parser_parse_template(parser);
	if (parser->errors->len > 0) {
		sds errmsg = NULL;
		while ((errmsg = vector_pop(parser->errors)) != NULL) {
			vector_push(env->errors, errmsg);
		}
		parser_destroy(parser);
		template_destroy(tmpl);
		return false;
	}
	parser_destroy(parser);
	hmap_sets(env->internal->templates, slice_whole(name), tmpl);
	return true;
}

bool
roscha_env_load_dir(struct roscha_env *env, const char *path)
{
	DIR *dir = opendir(path);
	if (!dir) {
		sds errmsg = sdscatfmt(sdsempty(), "unable to open dir %s, error %s",
				path, strerror(errno));
		vector_push(env->errors, errmsg);
		return false;
	}
	struct dirent *ent;
	while ((ent = readdir(dir))) {
		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) {
			continue;
		}

		struct stat fstats;
		sds fpath = sdscatfmt(sdsempty(), "%s/%s", path, ent->d_name);
		if (stat(fpath, &fstats)) {
			sds errmsg = sdscatfmt(sdsempty(),
					"unable to stat file %s, error %s", fpath, strerror(errno));
			vector_push(env->errors, errmsg);
			closedir(dir);
			return false;
		}
		if (S_ISDIR(fstats.st_mode)) continue;

		char *name = malloc(strlen(ent->d_name) + 1);
		strcpy(name, ent->d_name);

		int fd = open(fpath, O_RDONLY);
		char buf[BUFSIZE];
		sds body = sdsempty();
		size_t nread;
		while ((nread = read(fd, buf, BUFSIZE)) > 0) {
			buf[nread] = '\0';
			body = sdscat(body, buf);
		}
		close(fd);
		if (nread < 0) {
			sds errmsg = sdscatfmt(sdsempty(),
					"unable to read file %s, error %s", fpath, strerror(errno));
			vector_push(env->errors, errmsg);
			goto fileerr;
		}
		if (!roscha_env_add_template(env, name, body)) {
			goto fileerr;
		}
		continue;
fileerr:
		closedir(dir);
		sdsfree(body);
		return false;
	}

	closedir(dir);
	return true;
}

sds
roscha_env_render(struct roscha_env *env, const char *name)
{
	struct slice sname = slice_whole(name);
	return eval_template(env, &sname, NULL);
}

struct vector *
roscha_env_check_errors(struct roscha_env *env)
{
	if (env->errors->len > 0) return env->errors;
	return NULL;
}

void
roscha_env_destroy(struct roscha_env *env)
{
	size_t i;
	sds errmsg;
	vector_foreach (env->errors, i, errmsg) {
		sdsfree(errmsg);
	}
	vector_free(env->errors);
	roscha_object_unref(env->vars);
	hmap_destroy(env->internal->templates, roscha_env_destroy_templates_cb);
	free(env->internal);
	free(env);
}
