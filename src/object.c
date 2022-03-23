#include "object.h"

static const char *roscha_types[] = {
	"null",
	"int",
	"bool",
	"string",
	"vector",
	"hashmap",
};

static void
roscha_object_destroy_hmap_cb(const struct slice *key, void *val)
{
	struct roscha_object *obj = val;
	roscha_object_unref(obj);
}

extern inline const char *
roscha_type_print(enum roscha_type type)
{
	return roscha_types[type];
}

static inline sds
bool_string(bool val, sds str)
{
	sdscat(str, val ? "true" : "false");
	return str;
}

static inline sds
vector_string(struct vector *vec, sds str)
{
	size_t i;
	struct roscha_object *obj;
	str = sdscat(str, "[ ");
	vector_foreach(vec, i, obj) {
		str = roscha_object_string(obj, str);
		str = sdscat(str, ", ");
	}
	str = sdscat(str, "]");
	return str;
}

static inline sds
hmap_string(struct hmap *map, sds str)
{
	str = sdscat(str, "{ ");
	const struct slice *key;
	void *val;
	struct hmap_iter *iter = hmap_iter_new(map);
	hmap_iter_foreach(iter, &key, &val) {
		str = sdscatfmt(str, "\"%s\": ", key->str);
		const struct roscha_object *obj = val;
		str = roscha_object_string(obj, str);
		str = sdscat(str, ", ");
	}
	str = sdscat(str, "}");

	return str;
}

sds
roscha_object_string(const struct roscha_object *obj, sds str)
{
	switch (obj->type) {
	case ROSCHA_NULL:
		return sdscat(str, "null");
	case ROSCHA_BOOL:
		return bool_string(obj->boolean, str);
	case ROSCHA_INT:
		return sdscatfmt(str, "%I", obj->integer);
	case ROSCHA_STRING:
		return sdscat(str, obj->string);
	case ROSCHA_SLICE:
		return slice_string(&obj->slice, str);
	case ROSCHA_VECTOR:
		return vector_string(obj->vector, str);
	case ROSCHA_HMAP:
		return hmap_string(obj->hmap, str);
	}
	return str;
}

struct roscha_object *
roscha_object_new_int(int64_t val)
{
	struct roscha_object *obj = malloc(sizeof(*obj));
	obj->type = ROSCHA_INT;
	obj->refcount = 1;
	obj->integer = val;
	return obj;
}

struct roscha_object *
roscha_object_new_slice(struct slice s)
{
	struct roscha_object *obj = malloc(sizeof(*obj));
	obj->type = ROSCHA_SLICE;
	obj->refcount = 1;
	obj->slice = s;
	return obj;
}

struct roscha_object *
roscha_object_new_string(sds str)
{
	struct roscha_object *obj = malloc(sizeof(*obj));
	obj->type = ROSCHA_STRING;
	obj->refcount = 1;
	obj->string = str;
	return obj;
}

struct roscha_object *
roscha_object_new_vector(struct vector *vec)
{
	struct roscha_object *obj = malloc(sizeof(*obj));
	obj->type = ROSCHA_VECTOR;
	obj->refcount = 1;
	obj->vector = vec;
	return obj;
}

struct roscha_object *
roscha_object_new_hmap(struct hmap *map)
{
	struct roscha_object *obj = malloc(sizeof(*obj));
	obj->type = ROSCHA_HMAP;
	obj->refcount = 1;
	obj->hmap = map;
	return obj;
}

static inline void
roscha_object_vector_destroy(struct roscha_object *obj)
{
	size_t i;
	struct roscha_object *subobj;
	vector_foreach(obj->vector, i, subobj) {
		roscha_object_unref(subobj);
	}
	vector_free(obj->vector);
}

inline void
roscha_object_ref(struct roscha_object *obj)
{
	obj->refcount++;
}

void
roscha_object_unref(struct roscha_object *obj)
{
	if (obj == NULL) return;
	if (obj->type == ROSCHA_NULL || obj->type == ROSCHA_BOOL) return;
	if (--obj->refcount < 1) {
		switch (obj->type) {
		case ROSCHA_STRING:
			sdsfree(obj->string);
			break;
		case ROSCHA_VECTOR:
			roscha_object_vector_destroy(obj);
			break;
		case ROSCHA_HMAP:
			hmap_destroy(obj->hmap, roscha_object_destroy_hmap_cb);
			break;
		default:
			break;
		}
		free(obj);
	}
}

void
roscha_vector_push(struct roscha_object *vec, struct roscha_object *val)
{
	roscha_object_ref(val);
	vector_push(vec->vector, val);
}

struct roscha_object *
roscha_vector_pop(struct roscha_object *vec)
{
	return (struct roscha_object *)vector_pop(vec->vector);
}

struct roscha_object *
roscha_hmap_sets(struct roscha_object *hmap, struct slice key,
		struct roscha_object *value)
{
	roscha_object_ref(value);
	return hmap_sets(hmap->hmap, key, value);
}

struct roscha_object *
roscha_hmap_setstr(struct roscha_object *hmap, const char *key,
		struct roscha_object *value)
{
	roscha_object_ref(value);
	return hmap_set(hmap->hmap, key, value);
}

struct roscha_object *
roscha_hmap_gets(struct roscha_object *hmap, const struct slice *key)
{
	return (struct roscha_object *)hmap_gets(hmap->hmap, key);
}

struct roscha_object *
roscha_hmap_getstr(struct roscha_object *hmap, const char *key)
{
	return (struct roscha_object *)hmap_get(hmap->hmap, key);
}

struct roscha_object *
roscha_hmap_pops(struct roscha_object *hmap, const struct slice *key)
{
	return (struct roscha_object *)hmap_removes(hmap->hmap, key);
}

struct roscha_object *
roscha_hmap_popstr(struct roscha_object *hmap, 
		const char *key)
{
	return (struct roscha_object *)hmap_remove(hmap->hmap, key);
}

void
roscha_hmap_unsets(struct roscha_object *hmap, const struct slice *key)
{
	struct roscha_object *obj = hmap_removes(hmap->hmap, key);
	if (obj) roscha_object_unref(obj);
}

void
roscha_hmap_unsetstr(struct roscha_object *hmap, const char *key)
{
	struct roscha_object *obj = hmap_remove(hmap->hmap, key);
	if (obj) roscha_object_unref(obj);
}
