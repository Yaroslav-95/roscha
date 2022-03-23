#include "ast.h"
#include "slice.h"
#include "vector.h"

#include <stdio.h>
#include <string.h>

static inline sds
subblocks_string(struct vector *subblocks, sds str)
{
	size_t i;
	struct block *subblk;
	vector_foreach(subblocks, i, subblk) {
		str = block_string(subblk, str);
		str = sdscat(str, "\n");
	}

	return str;
}

static inline sds
ident_string(struct ident *ident, sds str)
{
	return slice_string(&ident->token.literal, str);
}

static inline sds
integer_string(struct integer *i, sds str)
{
	return sdscatfmt(str, "%I", i->value);
}

static inline sds
boolean_string(struct boolean *b, sds str)
{
	char *sval = b->value ? "true" : "false";
	return sdscatfmt(str, "%s", sval);
}

static inline sds
string_string(struct string *s, sds str)
{
	return slice_string(&s->value, str);
}

static inline sds
prefix_string(struct prefix *pref, sds str)
{
	str = sdscat(str, "(");
	str = slice_string(&pref->operator, str);
	str = expression_string(pref->right, str);
	str = sdscat(str, ")");
	return str;
}

static inline sds
infix_string(struct infix *inf, sds str)
{
	str = sdscat(str, "(");
	str = expression_string(inf->left, str);
	str = sdscat(str, " ");
	str = slice_string(&inf->operator, str);
	str = sdscat(str, " ");
	str = expression_string(inf->right, str);
	str = sdscat(str, ")");
	return str;
}

static inline sds
mapkey_string(struct indexkey *map, sds str)
{
	str = expression_string(map->left, str);
	str = sdscat(str, ".");
	str = ident_string(&map->key->ident, str);

	return str;
}

static inline sds
index_string(struct indexkey *index, sds str)
{
	str = expression_string(index->left, str);
	str = sdscat(str, "[");
	str = expression_string(index->key, str);
	str = sdscat(str, "]");

	return str;
}

sds
expression_string(struct expression *expr, sds str)
{
	switch (expr->type) {
	case EXPRESSION_IDENT:
		return ident_string(&expr->ident, str);
	case EXPRESSION_INT:
		return integer_string(&expr->integer, str);
	case EXPRESSION_BOOL:
		return boolean_string(&expr->boolean, str);
	case EXPRESSION_STRING:
		return string_string(&expr->string, str);
	case EXPRESSION_PREFIX:
		return prefix_string(&expr->prefix, str);
	case EXPRESSION_INFIX:
		return infix_string(&expr->infix, str);
	case EXPRESSION_MAPKEY:
		return mapkey_string(&expr->indexkey, str);
	case EXPRESSION_INDEX:
		return index_string(&expr->indexkey, str);
	}
	return str;
}

static inline sds
branch_string(struct branch *brnch, sds str)
{
	str = sdscat(str, "{% ");
	str = slice_string(&brnch->token.literal, str);
	str = sdscat(str, " ");
	str = expression_string(brnch->condition, str);
	str = sdscat(str, " ");
	str = sdscat(str, " %}\n");
	str = subblocks_string(brnch->subblocks, str);
	if (brnch->next) {
		str = branch_string(brnch->next, str);
	}
	return str;
}

static inline sds
cond_string(struct cond *cond, sds str)
{
	str = branch_string(cond->root, str);
	str = sdscat(str, "{% endif %}");
	return str;
}

static sds
loop_string(struct loop *loop, sds str)
{
	str = sdscat(str, "{% for ");
	str = ident_string(&loop->item, str);
	str = sdscat(str, " in ");
	str = expression_string(loop->seq, str);
	str = sdscat(str, " %}\n");
	str = subblocks_string(loop->subblocks, str);
	str = sdscat(str, "\n{% endfor %}");
	return str;
}

static inline sds
tblock_string(struct tblock *blk, sds str)
{
	str = sdscat(str, "{% block ");
	str = ident_string(&blk->name, str);
	str = subblocks_string(blk->subblocks, str);
	str = sdscat(str, " %}\n");
	str = sdscat(str, "\n{% endblock %}");
	return str;
}

static inline sds
parent_string(struct parent *prnt, sds str)
{
	str = sdscat(str, "{% extends ");
	str = string_string(prnt->name, str);
	str = sdscat(str, " %}");
	return str;
}

sds
tag_string(struct tag *tag, sds str)
{
	switch (tag->type) {
	case TAG_IF:
		return cond_string(&tag->cond, str);
	case TAG_FOR:
		return loop_string(&tag->loop, str);
	case TAG_BLOCK:
		return tblock_string(&tag->tblock, str);
	case TAG_EXTENDS:
		return parent_string(&tag->parent, str);
	case TAG_BREAK:
		str = sdscat(str, "{% ");
		str = slice_string(&tag->token.literal, str);
		return sdscat(str, " %}");
	default:
		break;
	}
	return str;
}

sds
variable_string(struct variable *var, sds str)
{
	str = sdscat(str, "{{ ");
	str = expression_string(var->expression, str);
	str = sdscat(str, " }}");
	return str;
}

sds
content_string(struct content *cnt, sds str)
{
	return slice_string(&cnt->token.literal, str);
}

sds
block_string(struct block *blk, sds str)
{
	switch (blk->type) {
	case BLOCK_CONTENT:
		return content_string(&blk->content, str);
	case BLOCK_VARIABLE:
		return variable_string(&blk->variable, str);
	case BLOCK_TAG:
		return tag_string(&blk->tag, str);
	default:
		break;
	}
	return str;
}

sds
template_string(struct template *tmpl, sds str)
{
	return subblocks_string(tmpl->blocks, str);
}

void
expression_destroy(struct expression *expr)
{
	switch (expr->type) {
	case EXPRESSION_PREFIX:
		expression_destroy(expr->prefix.right);
		break;
	case EXPRESSION_INFIX:
		expression_destroy(expr->infix.left);
		expression_destroy(expr->infix.right);
		break;
	case EXPRESSION_INDEX:
	case EXPRESSION_MAPKEY:
		expression_destroy(expr->indexkey.left);
		expression_destroy(expr->indexkey.key);
	case EXPRESSION_IDENT:
	case EXPRESSION_INT:
	case EXPRESSION_BOOL:
	case EXPRESSION_STRING:
	default:
		break;
	}
	free(expr);
}

static inline void
subblocks_destroy(struct vector *subblks)
{
	size_t i;
	struct block *blk;
	vector_foreach(subblks, i, blk){
		block_destroy(blk);
	}
	vector_free(subblks);
}

void
branch_destroy(struct branch *brnch)
{
	if (brnch->condition) expression_destroy(brnch->condition);
	subblocks_destroy(brnch->subblocks);
	if (brnch->next) branch_destroy(brnch->next);
	free(brnch);
}

void
tag_destroy(struct tag *tag)
{
	switch (tag->type) {
	case TAG_IF:
		branch_destroy(tag->cond.root);
		break;
	case TAG_FOR:
		expression_destroy(tag->loop.seq);
		subblocks_destroy(tag->loop.subblocks);
		break;
	case TAG_BLOCK:
		subblocks_destroy(tag->tblock.subblocks);
		break;
	case TAG_EXTENDS:
		free(tag->parent.name);
		break;
	case TAG_BREAK:
	default:
		break;
	}
}

void
block_destroy(struct block *blk)
{
	switch (blk->type) {
	case BLOCK_VARIABLE:
		expression_destroy(blk->variable.expression);
		break;
	case BLOCK_TAG:
		tag_destroy(&blk->tag);
		break;
	case BLOCK_CONTENT:
	default:
		break;
	}
	free(blk);
}

void
template_destroy(struct template *tmpl)
{
	free(tmpl->name);
	subblocks_destroy(tmpl->blocks);
	hmap_free(tmpl->tblocks);
	free(tmpl);
}
